/*
    Looperlative LP1 audio looper
    Copyright (C) 2005-2013 Robert Amstadt dba Looperlative Audio Products

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License only.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    Proprietary licensing of the software as modified only by Looperlative
    Audio Products is available.  Please contact Looperlative Audio Products
    for details.

    Email: licensing@looperlative.com

    US mail:	Looperlative Audio Products
    		6081 Meridian Ave. #70-315
		San Jose, CA 95120
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/proc_fs.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/param.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <asm/blackfin.h>

#ifdef BF533_SIMPLE_DMA
#include <asm/simple_bf533_dma.h>
#else
#include <asm/dma.h>
#endif

#include <linux/net.h>

#define LP1_MODULE_DEBUG

#ifdef LP1_MODULE_DEBUG
#define DDEBUG(x)	printk x
#else
#define DDEBUG(x)
#endif

extern unsigned long l1_data_A_sram_alloc(unsigned long size);

#define SSYNC __builtin_bfin_ssync()
#define CSYNC __builtin_bfin_csync()

#include "sport.h"
#include "loop_mem.h"
#include "loops.h"

#define USE_ZERO_CROSSING_DETECTOR	1
#define ZERO_THRESHOLD			128

#define SPORT_SAMPLING_RATE		44100
#define SPORT_MAJOR_NR			251

#define NUM_LOOPER_TIME_STAMPS	128
static long *looper_time_stamps;
static short looper_time_stamp_idx = 0;

static long looper_midi_clock_samples[1000];
static long looper_midi_clock_sample_idx = 0;

/*
 * Don't move the following two lines.  They are connected with each other.
 */
register long *svm asm ("P5");
#include "volume.h"

int sport_open(struct inode *inode, struct file *filp);
int sport_close(struct inode *inode, struct file *filp);
int sport_read(struct file *filp, char *buf, size_t count, loff_t *offp);
int sport_write(struct file *filp, const char *buf, 
		size_t count, loff_t *offp);
int sport_ioctl(struct inode *inode, struct file *filp, 
		uint cmd, unsigned long arg);
static long noise_gate(long sample_in, struct music_loop_s *loop, 
		       short channel);
static void record_track(long sample_in, struct music_loop_s *loop, 
			 struct music_track_s *track, short channel);
static void overdub_track(long sample_in, struct music_loop_s *loop, 
			  struct music_track_s *track, short channel);
static long replace_track(long sample_in, struct music_loop_s *loop, 
			  struct music_track_s *track, short channel);
static long fade_in(long sample_in, struct music_loop_s *loop, 
		    struct music_track_s *track, int initialize, 
		    short channel);
static long fade_out(long sample_in, struct music_loop_s *loop, 
		    struct music_track_s *track, int initialize, 
		     short channel);
static void sport_end_record(void);
static void sport_end_replace(void);
static void sport_trigger_cued(short stopped_track);
void play_fade_start(struct music_track_s *track);
void play_fade_stop(struct music_track_s *track);
void sport_stop_bouncing_tracks(void);
void sport_fade_loop_end(struct music_track_s *track);
void sport_dump_addresses(void);

static int sport_major = -1;
static struct file_operations sport_fops = 
{
    open:	sport_open,
    release:	sport_close,
    read:	sport_read,
    write:	sport_write,
    ioctl:	sport_ioctl
};

#define NUM_SAVED_MIDI_COUNTERS		32

static struct sport_s
{
    short next_fx_bufn;
    long noise_gate_level;	/* Minimum signal level */
    short noise_gate_delay;	/* Time required to detect noise */
    short noise_gate_time[2];	/* Time since last value above gate level */
    short noise_gate_release[2];/* Release timer */
    short current_track;
    short master_volume;
    struct music_loop_s *loop;
    long max_pos_value;
    long min_pos_value;
    long max_out_value;

    short beats_per_measure;	/* Quarter notes per measure */
    short midi_clk_error;	/* 1 if last midi time far from average. */
    short midi_clk_detect;	/* 1 if midi clock detected. */
    short saved_midi_counters_idx;
    long num_midi_clk_error;
    long cur_midi_counter;	/* Quarter note time counter */
    long saved_midi_counters[NUM_SAVED_MIDI_COUNTERS];

    short wet_only;		/* Wet or wet + dry */
    short aux1_mask;		/* Loops assigned to AUX 1 */
    short aux2_mask;		/* Loops assigned to AUX 1 */

    long next_clock_pos;	/* Next pos to send MIDI clock out */
    short clock_segment;
    short send_clock;		/* flag to send MIDI clock */
    short midi_sync_out_enabled;
    long volume_change_threshold;
    long record_level_threshold;
}
sport_info = 
{
    0, 		// next_fx_bufn
    20, 	// noise_gate_level
    1000, 	// noise_gate_delay
    { 0, 0 }, 	// noise_gate_time
    { 0, 0 }, 	// noise_gate_release
    0, 		// current_track
    0, 		// master_volume
    NULL, 	// loop
    0, 		// max_pos_value
    0, 		// min_pos_value
    0, 		// max_out_value

    4, 		// beats_per_measure
    0, 		// midi_clk_error
    0, 		// midi_clk_detect
    0, 		// saved_midi_counters_idx
    0,		// num_midi_clk_error
    0, 		// cur_midi_counter
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },

    0, 		// wet_only
    0, 		// aux1_mask
    0, 		// aux2_mask

    0, 		// next_clock_pos
    0, 		// clock_segment
    0, 		// send_clock
    0, 		// midi_sync_out_enabled
    (2L << 24), // volume_change_threshold
    0L		// record_level_threshold
};

static struct music_loop_s audio_loop;

volatile long *sport_dma_buf;	/* Allocate one large block and divide */
volatile long *sport_rx_sg;	/* Allocate one large block and divide */
volatile long *sport1_dma_buf;	/* Allocate one large block and divide */
volatile long *sport1_rx_sg;	/* Allocate one large block and divide */

#ifdef USE_SPORT_OUT_FILTER
long sport_out_filter[2];
#endif

volatile unsigned short sample_counter = 0;

static spinlock_t sport_dma_lock = SPIN_LOCK_UNLOCKED;
// static short dma_level = 0;

static inline long sport_avg_midi_counter(void)
{
    return (sport_info.saved_midi_counters[0] + 
	    sport_info.saved_midi_counters[1] + 
	    sport_info.saved_midi_counters[2] + 
	    sport_info.saved_midi_counters[3] + 
	    sport_info.saved_midi_counters[4] + 
	    sport_info.saved_midi_counters[5] + 
	    sport_info.saved_midi_counters[6] + 
	    sport_info.saved_midi_counters[7] + 
	    sport_info.saved_midi_counters[8] + 
	    sport_info.saved_midi_counters[9] + 
	    sport_info.saved_midi_counters[10] + 
	    sport_info.saved_midi_counters[11] + 
	    sport_info.saved_midi_counters[12] + 
	    sport_info.saved_midi_counters[13] + 
	    sport_info.saved_midi_counters[14] + 
	    sport_info.saved_midi_counters[15]) / 16;
}

static short sport_check_clock_out(void)
{
    short flag = sport_info.send_clock;

    if (sport_info.send_clock >= 2)
	sport_info.send_clock = 1;
    else
	sport_info.send_clock = 0;

    return flag;
}

int sport_open(struct inode *inode, struct file *filp)
{
    struct sport_track_file_s *privdata = NULL;
    int error = 0;

    sport_dump_addresses();

    if (MINOR(inode->i_rdev) > 0 && MINOR(inode->i_rdev) <= 8)
    {
	privdata = kmalloc(sizeof(*privdata), GFP_KERNEL);
	
	if (privdata)
	{
	    struct music_track_s *track;

	    privdata->track = MINOR(inode->i_rdev) - 1;
	    track = &sport_info.loop->tracks[privdata->track];

	    privdata->pos = track->virtual_start_offset;
	    while (privdata->pos < 0)
		privdata->pos += track->length;
	    track->virtual_start_offset = privdata->pos;
	    privdata->read_count = 0;
	}
	else
	    error = EIO;
    }

    filp->private_data = (void *) privdata;
    return error;
}

int sport_close(struct inode *inode, struct file *filp)
{
    if (filp->private_data)
	kfree(filp->private_data);
    filp->private_data = NULL;
    
    return 0;
}

int sport_read(struct file *filp, char *buf, size_t count, loff_t *offp)
{
    static unsigned long rawpos = 0;
    
    struct sport_track_file_s *privdata = filp->private_data;
    struct music_track_s *track;
    short block;
    short offset;
    int n;
    size_t nread = 0;

    if (privdata == NULL)
    {
	if (rawpos > 90 * 1024 * 1024)
	    rawpos = 0;
	
	copy_to_user(buf, (char *) ((32 * 1024 * 1024) + rawpos), count);
	rawpos += count;

	return count;
    }

    track = &sport_info.loop->tracks[privdata->track];
    if (track == NULL)
	return -EIO;

    if (privdata->pos < 0)
	return 0;

    while (count > 0)
    {
	block = privdata->pos / LOOPER_BLOCK_SIZE;
	offset = privdata->pos - (block * LOOPER_BLOCK_SIZE);

	n = LOOPER_BLOCK_SIZE - offset;

	if (count < n)
	    n = count;

	if (track->length - privdata->pos < n)
	    n = track->length - privdata->pos;

	if (track->length - privdata->read_count < n)
	    n = track->length - privdata->read_count;

	if (n == 0)
	    break;
	
	if (track->audio[block])
	    copy_to_user(buf, (char *) track->audio[block] + offset, n);
	else
	{
	    if (access_ok(VERIFY_WRITE, buf, n))
		memset(buf, 0, n);
	}
	
	buf += n;
	count -= n;
	nread += n;
	privdata->read_count += n;

	privdata->pos += n;
	if (privdata->pos >= track->length)
	    privdata->pos = 0;
	else if (privdata->pos == track->virtual_start_offset)
	{
	    privdata->pos = -1;
	    break;
	}
    }

    return nread;
}

int sport_write(struct file *filp, const char *buf, 
		size_t count, loff_t *offp)
{
    struct sport_track_file_s *privdata = filp->private_data;
    struct music_track_s *track;
    short block;
    short offset;
    int n;
    int nwritten = 0;

    if (privdata == NULL)
	return -EIO;

    track = &sport_info.loop->tracks[privdata->track];
    if (track == NULL)
	return -EIO;

    if (privdata->track == sport_info.loop->record_track ||
	privdata->track == sport_info.loop->bounce_track ||
	privdata->track == sport_info.loop->overdub_track)
    {
	return -EIO;
    }

    track->options = 0;
    track->pos = 0;
    track->virtual_start_offset = 0;

    while (count > 0)
    {
	block = privdata->pos / LOOPER_BLOCK_SIZE;
	offset = privdata->pos - (block * LOOPER_BLOCK_SIZE);

	n = LOOPER_BLOCK_SIZE - offset;
	if (count < n)
	    n = count;
	
	if (n == 0)
	    break;
	
	if (track->audio[block] == NULL)
	    track->audio[block] = (loop_audio_t *) looper_alloc();
	if (track->audio[block] == NULL)
	    break;

	copy_from_user((char *) track->audio[block] + offset, buf, n);
	
	buf += n;
	privdata->pos += n;
	count -= n;
	nwritten += n;

	track->length = privdata->pos;
	track->original_length = privdata->pos;
    }

    return nwritten;
}

void sport_quantized_wait(struct music_track_s *t)
{
    long delta;			/* segment size */
    long wait_pos;		/* Position we're waiting to reach */
    long start_pos;		/* Position we started at */

    delta = t->length / t->quantized_steps;
    start_pos = t->pos;

    if (t->options & MUSIC_TRACK_OPTION_REVERSE)
    {
	wait_pos = ((start_pos / delta) - 1) * delta;
	if (wait_pos < 0)
	    wait_pos += t->length;
	    
	while (t->pos > wait_pos && t->pos <= start_pos)
	    ;
    }
    else
    {
	wait_pos = ((start_pos / delta) + 1) * delta;
	while (t->pos < wait_pos && t->pos >= start_pos)
	    ;
    }
}

int sport_ioctl_immedstop(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    short li;

    if ((s->loop->record_track >= 0 && arg - 1 == s->loop->record_track) ||
	(s->loop->bounce_track >= 0 && arg - 1 == s->loop->bounce_track))
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	sport_end_record();
	sport_trigger_cued(arg);
	return 0;
    }

    if (s->loop->overdub_track >= 0 && arg - 1 == s->loop->overdub_track)
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	s->loop->overdub_track = -1;
	play_fade_stop(&s->loop->tracks[arg-1]);
	sport_trigger_cued(arg);
	return 0;
    }

    if (s->loop->replace_track >= 0 && arg - 1 == s->loop->replace_track)
    {
	if (s->loop->tracks[arg-1].options & MUSIC_TRACK_OPTION_QUANTIZED)
	{
	    sport_quantized_wait(&s->loop->tracks[arg-1]);
	    s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_QUANTIZED;
	}
	
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	sport_end_replace();
	sport_trigger_cued(arg);
	return 0;
    }

    if (arg >= 1 && arg <= 8)
    {
	if (s->loop->tracks[arg-1].options & MUSIC_TRACK_OPTION_PLAYING)
	    play_fade_stop(&s->loop->tracks[arg-1]);
	    
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
    }
    else if (arg == 0)	/* All stop */
    {
	for (li = 0; li < 8; li++)
	{
	    if (s->loop->tracks[li].options & MUSIC_TRACK_OPTION_PLAYING)
	    {
		if (arg == 0)
		    arg = li + 1;
		
		play_fade_stop(&s->loop->tracks[li]);
	    }
	    
	    s->loop->tracks[li].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	    s->loop->tracks[li].options &= ~MUSIC_TRACK_OPTION_CUED;
	}
    }

    sport_trigger_cued(arg);
    return 0;
}

int sport_ioctl_delayedstop(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    short li;

    if ((s->loop->record_track >= 0 && arg - 1 == s->loop->record_track) ||
	(s->loop->bounce_track >= 0 && arg - 1 == s->loop->bounce_track))
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	sport_end_record();
	sport_trigger_cued(arg);
	return 0;
    }

    if (s->loop->overdub_track >= 0 && arg - 1 == s->loop->overdub_track)
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	s->loop->overdub_track = -1;
	play_fade_stop(&s->loop->tracks[arg-1]);
	sport_trigger_cued(arg);
	return 0;
    }

    if (s->loop->replace_track >= 0 && arg - 1 == s->loop->replace_track)
    {
	if (s->loop->tracks[arg-1].options & MUSIC_TRACK_OPTION_QUANTIZED)
	{
	    sport_quantized_wait(&s->loop->tracks[arg-1]);
	    s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_QUANTIZED;
	}
	
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	sport_end_replace();
	sport_trigger_cued(arg);
	return 0;
    }

    if (arg >= 1 && arg <= 8)
    {
	s->loop->tracks[arg-1].options |= MUSIC_TRACK_OPTION_DELAYEDSTOP;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_DELAYEDPLAY;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
    }
    else if (arg == 0)	/* All stop */
    {
	for (li = 0; li < 8; li++)
	{
	    s->loop->tracks[li].options |= MUSIC_TRACK_OPTION_DELAYEDSTOP;
	    s->loop->tracks[li].options &= ~MUSIC_TRACK_OPTION_DELAYEDPLAY;
	    s->loop->tracks[li].options &= ~MUSIC_TRACK_OPTION_CUED;
	}
    }

    return 0;
}

int sport_ioctl_immedplay(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    short li;
    short allstopped = 0;
    short retrigger = 0;

    /* Retrigger? */
    if (arg & 0x100)
	retrigger = 1;
    
    arg &= ~0xff00;

    /* */
    if ((s->loop->record_track >= 0 && arg - 1 == s->loop->record_track) ||
	(s->loop->bounce_track >= 0 && arg - 1 == s->loop->bounce_track))
    {
	sport_end_record();
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	return 0;
    }

    if (s->loop->overdub_track >= 0 && arg - 1 == s->loop->overdub_track)
    {
	s->loop->overdub_track = -1;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	return 0;
    }

    if (s->loop->replace_track >= 0 && arg - 1 == s->loop->replace_track)
    {
	if (s->loop->tracks[arg-1].options & MUSIC_TRACK_OPTION_QUANTIZED)
	{
	    sport_quantized_wait(&s->loop->tracks[arg-1]);
	    s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_QUANTIZED;
	}
	
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	sport_end_replace();
	return 0;
    }

    /* If all stopped then start immediately */
    for (li = 0; li < 8; li++)
	if (s->loop->tracks[li].options & MUSIC_TRACK_OPTION_PLAYING)
	    break;
    
    if (li == 8)
	allstopped = 1;

    if (arg >= 1 && arg <= 8 && s->loop->tracks[arg-1].length != 0)
    {
	struct music_track_s *track = &s->loop->tracks[arg-1];

	if (!(s->loop->tracks[arg-1].options & MUSIC_TRACK_OPTION_PLAYING))
	{
	    if (retrigger)
	    {
		track->pos = track->virtual_start_offset;
		while (track->pos < 0)
		    track->pos += track->length;
	    }
	    
	    play_fade_start(&s->loop->tracks[arg-1]);
	}

	/* 
	 * If we are retriggering a playing track, then we set the
	 * retrigger flag and stop the track.  The track will automatically
	 * restart after it stops.
	 */
	else if (retrigger)
	{
	    track->options |= MUSIC_TRACK_OPTION_RETRIGGER;
	    play_fade_stop(track);
	    track->options &= ~MUSIC_TRACK_OPTION_PLAYING;
	    track->options &= ~MUSIC_TRACK_OPTION_CUED;
	    return 0;
	}
	
	track->options |= MUSIC_TRACK_OPTION_PLAYING;
	track->options &= ~MUSIC_TRACK_OPTION_CUED;

	/* MIDI sync track?  If so, no automatic retrigger on all stopped */
	if (track->options & MUSIC_TRACK_OPTION_MIDISYNC)
	    allstopped = 0;

	if (allstopped)
	{
	    for (li = 0; li < 8; li++)
	    {
		/* Do not shift if empty or MIDI sync'ed */
		if (s->loop->tracks[li].length == 0 ||
		    (s->loop->tracks[li].options & 
		     MUSIC_TRACK_OPTION_MIDISYNC))
		{
		    continue;
		}

		s->loop->tracks[li].pos = 
		    s->loop->tracks[li].virtual_start_offset;
		while (s->loop->tracks[li].pos < 0)
		    s->loop->tracks[li].pos += s->loop->tracks[li].length;

		if (li == 0)
		{
		    sport_info.clock_segment = 0;
		    sport_info.next_clock_pos = 0;
		}
	    }
	}
    }
    else if (arg == 0)	/* All play */
    {
	for (li = 0; li < 8; li++)
	{
	    if (s->loop->tracks[li].length == 0)
		continue;
	    
	    if (!(s->loop->tracks[li].options & MUSIC_TRACK_OPTION_PLAYING))
		play_fade_start(&s->loop->tracks[li]);
	
	    s->loop->tracks[li].options |= MUSIC_TRACK_OPTION_PLAYING;
	    s->loop->tracks[li].options &= ~MUSIC_TRACK_OPTION_CUED;
	    if (retrigger || 
		(allstopped && 
		 !(s->loop->tracks[li].options & MUSIC_TRACK_OPTION_MIDISYNC)))
	    {
		s->loop->tracks[li].pos = 
		    s->loop->tracks[li].virtual_start_offset;
		
		while (s->loop->tracks[li].pos < 0)
		    s->loop->tracks[li].pos += s->loop->tracks[li].length;

		if (li == 0)
		{
		    sport_info.clock_segment = 0;
		    sport_info.next_clock_pos = 0;
		}
	    }
	}
    }

    return 0;
}

int sport_ioctl_delayedplay(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    short li;

    arg &= 0xff;

    if ((s->loop->record_track >= 0 && arg - 1 == s->loop->record_track) ||
	(s->loop->bounce_track >= 0 && arg - 1 == s->loop->bounce_track))
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	sport_end_record();
	return 0;
    }

    if (s->loop->overdub_track >= 0 && arg - 1 == s->loop->overdub_track)
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	s->loop->overdub_track = -1;
	return 0;
    }

    if (s->loop->replace_track >= 0 && arg - 1 == s->loop->replace_track)
    {
	if (s->loop->tracks[arg-1].options & MUSIC_TRACK_OPTION_QUANTIZED)
	{
	    sport_quantized_wait(&s->loop->tracks[arg-1]);
	    s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_QUANTIZED;
	}
	
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	sport_end_replace();
	return 0;
    }

    if (arg >= 1 && arg <= 8 && s->loop->tracks[arg-1].length != 0)
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	s->loop->tracks[arg-1].options |= MUSIC_TRACK_OPTION_DELAYEDPLAY;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_DELAYEDSTOP;
    }
    else if (arg == 0)	/* All play */
    {
	for (li = 0; li < 8; li++)
	{
	    if (s->loop->tracks[li].length == 0)
		continue;
	    
	    s->loop->tracks[li].options &= ~MUSIC_TRACK_OPTION_CUED;
	    s->loop->tracks[li].options |= MUSIC_TRACK_OPTION_DELAYEDPLAY;
	    s->loop->tracks[li].options &= ~MUSIC_TRACK_OPTION_DELAYEDSTOP;
	}
    }

    return 0;
}

int sport_ioctl_bounce(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct sport_bounce_s *bp = (struct sport_bounce_s *) arg;
    struct music_track_s *t;
    struct music_track_s *mt = &s->loop->tracks[s->loop->master_track];
    short dt = bp->destination_track - 1;
    short st;

    if (s->loop->bounce_track >= 0 && dt == s->loop->bounce_track)
    {
	s->loop->bounce_options |= MUSIC_BOUNCE_OPTION_ENDRECORD;
	return 0;
    }

    if (s->loop->record_track >= 0 || 
	s->loop->bounce_track >= 0 || 
	s->loop->overdub_track >= 0 ||
	s->loop->replace_track >= 0 ||
	dt < 0 || dt >= 8 ||
	bp->source_mask == 0)
    {
	return -EINVAL;
    }

    /*
      Mark bounce sources.
    */
    for (st = 0; st < 8; st++)
    {
	if (bp->source_mask & (1 << st) && st != dt)
	    s->loop->tracks[st].options |= MUSIC_TRACK_OPTION_BOUNCEFROM;
	else
	    s->loop->tracks[st].options &= ~MUSIC_TRACK_OPTION_BOUNCEFROM;
    }

    s->loop->bounce_options = 0;
    s->loop->bounce_options |= MUSIC_BOUNCE_OPTION_MIXINPUT;

    /*
      Start recording on bounce track.
     */
    t = &s->loop->tracks[dt];

    t->options &= ~MUSIC_TRACK_OPTION_CUED;
    t->options &= ~MUSIC_TRACK_OPTION_REVERSE;
    t->options |= MUSIC_TRACK_OPTION_PLAYING;

    t->original_length = 0;

    if (bp->sync_type == SPORT_SYNC_MIDI)
    {
	t->options |= MUSIC_TRACK_OPTION_MIDISYNC;

	t->quarternote_period = (sport_avg_midi_counter() * 24) >> (MIDI_FRACTION_BITS + 1);
	t->midi_sync_count = sport_info.cur_midi_counter;
	t->midiclock_period = sport_avg_midi_counter();
	t->midiclock_offset = sport_info.cur_midi_counter;

	t->original_length = ((long) (SPORT_DMA_BYTES * 
				      sport_info.beats_per_measure) * 
			      t->quarternote_period);
	t->length = t->original_length;
	t->length_check_point = t->original_length / 4;

	t->virtual_start_offset = 0;
    }
    else if (bp->sync_type == SPORT_SYNC_INTERNAL &&
	s->loop->master_track >= 0 &&
	s->loop->master_track != dt &&
	mt->length != 0)
    {
	int vpos = mt->pos + mt->virtual_start_offset;
	
	t->options |= MUSIC_TRACK_OPTION_SYNC;
	t->original_length = mt->length;

	if (vpos < (mt->length - vpos))
	    t->virtual_start_offset = -vpos;
	else
	    t->virtual_start_offset = mt->length - vpos;
    }
    else
	t->virtual_start_offset = 0;

    t->length = 0;
    t->pos = 0;

    if (s->loop->master_track < 0)
	s->loop->master_track = dt;

    s->loop->bounce_track = dt;
    if (s->loop->bounce_track == 0)
    {
	sport_info.clock_segment = 0;
	sport_info.next_clock_pos = 0;
    }

    s->loop->undo_track = -1;

    return 0;
}

int sport_ioctl_record(unsigned long arg)
{
    struct sport_s *s = &sport_info;

    if (s->loop->record_track >= 0 || 
	s->loop->bounce_track >= 0 || 
	s->loop->overdub_track >= 0 ||
	s->loop->replace_track >= 0)
    {
	return -EINVAL;
    }

    if (arg >= 1 && arg <= 8)
    {
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_CUED;
	s->loop->tracks[arg-1].options &= ~MUSIC_TRACK_OPTION_REVERSE;
	s->loop->tracks[arg-1].options |= MUSIC_TRACK_OPTION_PLAYING;
	s->loop->tracks[arg-1].length = 0;
	s->loop->tracks[arg-1].original_length = 0;
	s->loop->tracks[arg-1].virtual_start_offset = 0;
	s->loop->tracks[arg-1].pos = 0;

	if (s->loop->master_track < 0)
	    s->loop->master_track = arg - 1;

	s->loop->record_track = arg - 1;
	if (s->loop->record_track == 0)
	{
	    sport_info.clock_segment = 0;
	    sport_info.next_clock_pos = 0;
	}
	
	s->loop->undo_track = -1;
    }
    else
	return -EINVAL;

    return 0;
}

int sport_ioctl_midisync_record(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;

    if (s->loop->record_track >= 0 || 
	s->loop->overdub_track >= 0 ||
	s->loop->replace_track >= 0)
    {
	return -EINVAL;
    }

    if (arg >= 1 && arg <= 8 && sport_avg_midi_counter() > 0)
    {
	t = &s->loop->tracks[arg-1];
	
	t->options &= ~MUSIC_TRACK_OPTION_CUED;
	t->options &= ~MUSIC_TRACK_OPTION_REVERSE;
	t->options |= (MUSIC_TRACK_OPTION_PLAYING|MUSIC_TRACK_OPTION_MIDISYNC);
	t->pos = 0;
	t->virtual_start_offset = 0;

	t->quarternote_period = (sport_avg_midi_counter() * 24) >> (MIDI_FRACTION_BITS + 1);
	t->midi_sync_count = sport_info.cur_midi_counter;
	t->midiclock_period = sport_avg_midi_counter();
	t->midiclock_offset = sport_info.cur_midi_counter;

	t->original_length = ((long) (SPORT_DMA_BYTES * 
				      sport_info.beats_per_measure) * 
			      t->quarternote_period);
	t->length = t->original_length;
	t->length_check_point = t->original_length / 4;

	if (s->loop->master_track < 0)
	    s->loop->master_track = arg - 1;

	s->loop->record_track = arg - 1;
	if (s->loop->record_track == 0)
	{
	    sport_info.clock_segment = 0;
	    sport_info.next_clock_pos = 0;
	}

	s->loop->undo_track = -1;
    }
    else
	return -EINVAL;

    return 0;
}

int sport_ioctl_sync_record(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct music_track_s *mt = &s->loop->tracks[s->loop->master_track];
    int vpos;

    if (s->loop->master_track < 0 || 
	s->loop->master_track == arg - 1 ||
	mt->length == 0)
    {
	return sport_ioctl_record(arg);
    }

    if (s->loop->record_track >= 0 || 
	s->loop->overdub_track >= 0 ||
	s->loop->replace_track >= 0 ||
	arg < 1 || 
	arg > 8)
    {
	return -EINVAL;
    }

    t = &s->loop->tracks[arg-1];
	
    t->options &= ~MUSIC_TRACK_OPTION_CUED;
    t->options &= ~MUSIC_TRACK_OPTION_REVERSE;
    t->options |= (MUSIC_TRACK_OPTION_PLAYING|MUSIC_TRACK_OPTION_SYNC);
    t->length = 0;
    t->original_length = mt->length;
    t->pos = 0;

    vpos = mt->pos + mt->virtual_start_offset;
	
    if (vpos < (mt->length - vpos))
	t->virtual_start_offset = -vpos;
    else
	t->virtual_start_offset = mt->length - vpos;

    s->loop->record_track = arg - 1;

    s->loop->undo_track = -1;

    return 0;
}

int sport_ioctl_quantizedreplace(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    unsigned long replace_option = 0;

    if (s->loop->record_track >= 0 ||
	s->loop->bounce_track >= 0 ||
	s->loop->overdub_track >= 0 ||
	s->loop->replace_track >= 0)
    {
	return -EINVAL;
    }

    if (arg & 0x100)
	replace_option = MUSIC_TRACK_OPTION_REPLACEPLUS;

    arg &= 0xff;
    if (arg >= 1 && arg <= 8)
    {
	t = &s->loop->tracks[arg - 1];

	if (t->length == 0)
	    return -EINVAL;
	
	if (!(t->options & MUSIC_TRACK_OPTION_PLAYING))
	    play_fade_start(t);

	sport_quantized_wait(t);
	
	t->options &= ~(MUSIC_TRACK_OPTION_CUED | 
			MUSIC_TRACK_OPTION_REPLACEPLUS);
	t->options |= MUSIC_TRACK_OPTION_PLAYING;
	t->options |= MUSIC_TRACK_OPTION_QUANTIZED;
	t->options |= replace_option;

	t->replace_blend_level = 0;
	t->replace_blend_direction = -1;
	s->loop->replace_track = arg - 1;
    }
    else
	return -EINVAL;

    return 0;
}

int sport_ioctl_replace(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    unsigned long replace_option = 0;

    if (s->loop->record_track >= 0 ||
	s->loop->bounce_track >= 0 ||
	s->loop->overdub_track >= 0 ||
	s->loop->replace_track >= 0)
    {
	return -EINVAL;
    }

    if (arg & 0x100)
	replace_option = MUSIC_TRACK_OPTION_REPLACEPLUS;

    arg &= 0xff;
    if (arg >= 1 && arg <= 8)
    {
	t = &s->loop->tracks[arg - 1];

	if (t->length == 0)
	    return -EINVAL;
	
	if (!(t->options & MUSIC_TRACK_OPTION_PLAYING))
	    play_fade_start(t);
	
	t->options &= ~(MUSIC_TRACK_OPTION_CUED | 
			MUSIC_TRACK_OPTION_REPLACEPLUS);
	t->options |= MUSIC_TRACK_OPTION_PLAYING;
	t->options |= replace_option;

	t->replace_blend_level = 0;
	t->replace_blend_direction = -1;
	s->loop->replace_track = arg - 1;
    }
    else
	return -EINVAL;

    return 0;
}

static void sport_end_replace(void)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    unsigned long flags;

    spin_lock_irqsave(&sport_dma_lock, flags);

    if (s->loop->replace_track >= 0)
    {
	t = &s->loop->tracks[s->loop->replace_track];

	t->replace_blend_level = -1023;
	t->replace_blend_direction = 1;
	s->loop->replace_track = -1;
    }

    spin_unlock_irqrestore(&sport_dma_lock, flags);
}
    
int sport_ioctl_erase(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct music_track_s *mt = &s->loop->tracks[s->loop->master_track];
    int track_num = (arg & CMD_SPORT_OPTION_TRACKNUM);
    int b;
    int nblocks;
    unsigned long flags;

    if ((arg & CMD_SPORT_OPTION_FILL) && 
	(s->loop->master_track < 0 || 
	 s->loop->master_track == track_num - 1 ||
	 mt->length == 0))
    {
	return -EINVAL;
    }

    if (track_num >= 1 && track_num <= 8)
    {
	t = &s->loop->tracks[track_num - 1];
	
	t->options = 0;
	t->length = 0;
	t->pos = 0;
	t->original_length = 0;
	t->virtual_start_offset = 0;
 	t->fadein = 0;
	t->fadein_level[0] = 0;
	t->fadein_level[1] = 0;
	t->fadeout = 0;
	t->fadeout_level[0] = 0;
	t->fadeout_level[1] = 0;
	t->slow_cycle_inc = 400;
	t->slow_cycle = 0;

	if (s->loop->bounce_track == track_num - 1)
	    s->loop->bounce_track = -1;
	if (s->loop->overdub_track == track_num - 1)
	    s->loop->overdub_track = -1;
	if (s->loop->record_track == track_num - 1)
	    s->loop->record_track = -1;
	if (s->loop->replace_track == track_num - 1)
	    s->loop->replace_track = -1;
	
	b = 0;

	if ((arg & CMD_SPORT_OPTION_FILL))
	{
	    nblocks = (mt->length / LOOPER_BLOCK_SIZE) + 1;

	    /* zero any currently allocated block */
	    while (b < nblocks && t->audio[b])
	    {
		memset(t->audio[b], 0, LOOPER_BLOCK_SIZE);
		b++;
	    }

	    while (b < nblocks)
	    {
		spin_lock_irqsave(&sport_dma_lock, flags);

		t->audio[b] = (loop_audio_t *) looper_alloc();

		spin_unlock_irqrestore(&sport_dma_lock, flags);

		if (t->audio[b] == NULL)
		    break;
		
		b++;
	    }

	    /* Did we run out of memory? */
	    if (b < nblocks)
		b = 0;
	    else
	    {
		t->pos = mt->pos;
		t->options |= MUSIC_TRACK_OPTION_PLAYING;
		t->original_length = mt->original_length;
		t->length = mt->length;
		t->virtual_start_offset = mt->virtual_start_offset;
	    }
	}

	/* Free any unused blocks */
	while (b < LOOPER_MAX_BLOCKS && t->audio[b] != NULL)
	{
	    looper_free((void *) t->audio[b]);
	    t->audio[b] = NULL;
	    b++;
	}
    }
    else
	return -EINVAL;

    return 0;
}

int sport_ioctl_loopstatus(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct sport_loop_status_s ls;
    unsigned long status = 0;
    short i;
    unsigned long flags;

    t = s->loop->tracks;
    for (i = 0; i < 8; i++, t++)
    {
	spin_lock_irqsave(&sport_dma_lock, flags);
	ls.midi_error[i] = t->midi_error;
	spin_unlock_irqrestore(&sport_dma_lock, flags);

	if (t->options & MUSIC_TRACK_OPTION_BOUNCEFADE)
	{
	    sport_fade_loop_end(t);
	    t->options &= ~MUSIC_TRACK_OPTION_BOUNCEFADE;
	}
	
	if (t->length > 0)
	{
	    if (s->loop->record_track == i ||
		((s->loop->bounce_track == i) &&
		 !(s->loop->bounce_options & MUSIC_BOUNCE_OPTION_ENDRECORD)))
	    {
		status |= (SPORT_TRACK_STATUS_RECORDING << (i * 4));
	    }
	    else if (s->loop->overdub_track == i ||
		     ((s->loop->bounce_track == i) &&
		      (s->loop->bounce_options & 
		       MUSIC_BOUNCE_OPTION_ENDRECORD)))
	    {
		status |= (SPORT_TRACK_STATUS_OVERDUBBING << (i * 4));
	    }
	    else if (s->loop->replace_track == i)
		status |= (SPORT_TRACK_STATUS_REPLACING << (i * 4));
	    else if (t->options & MUSIC_TRACK_OPTION_PLAYING)
		status |= (SPORT_TRACK_STATUS_PLAYING << (i * 4));
	    else
		status |= (SPORT_TRACK_STATUS_STOPPED << (i * 4));
	}
	else if (s->loop->record_track == i)
	{
	    status |= (SPORT_TRACK_STATUS_ARMED << (i * 4));
	}

	ls.track_position[i] = t->pos + t->virtual_start_offset;
	if (ls.track_position[i] < 0)
	    ls.track_position[i] += t->length;
	else if (ls.track_position[i] > t->length)
	    ls.track_position[i] -= t->length;

	if (t->length == 0)
	    ls.track_position[i] = 0;

	ls.track_length[i] = t->length;
	ls.track_speeds[i] = t->slow_cycle_inc;
    } 

    ls.track_states = status;
    ls.master_track = s->loop->master_track + 1;
    ls.max_pos_value = sport_info.max_pos_value;
    ls.min_pos_value = sport_info.min_pos_value;
    ls.max_out_value = sport_info.max_out_value;
    ls.noise_gate_level = sport_info.noise_gate_level;
    ls.midi_period = (sport_avg_midi_counter() * 24) >> (MIDI_FRACTION_BITS + 1);
    
    sport_info.max_pos_value = 0;
    sport_info.max_out_value = 0;
    sport_info.min_pos_value = 0x7fffffff;
    
    copy_to_user((void *) arg, &ls, sizeof(ls));

    return 0;
}

int sport_ioctl_setmaster(unsigned long arg)
{
    struct sport_s *s = &sport_info;

    if (arg < 0 || arg > 8)
	return -EINVAL;

    s->loop->master_track = arg - 1;

    return 0;
}

int sport_ioctl_tracklevel(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct sport_track_level_s *tl = (struct sport_track_level_s *) arg;
    
    if (tl->track < 1 || tl->track > 8 || tl->level < -128 || tl->level > 128)
	return -EINVAL;

    t = &s->loop->tracks[tl->track - 1];
    t->target_level = tl->level * 4;

    if (t->pan >= 0)
    {
	t->per_channel_targets[0] = t->target_level - t->pan;
	t->per_channel_targets[1] = t->target_level;
    }
    else if (t->pan < 0)
    {
	t->per_channel_targets[0] = t->target_level;
	t->per_channel_targets[1] = t->target_level + t->pan;
    }
    
    return 0;
}

int sport_ioctl_pan(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct sport_track_level_s *tl = (struct sport_track_level_s *) arg;
    
    if (tl->track < 1 || tl->track > 8 || tl->level < -128 || tl->level > 128)
	return -EINVAL;

    t = &s->loop->tracks[tl->track - 1];
    t->pan = tl->level * 4;

    if (t->pan >= 0)
    {
	t->per_channel_targets[0] = t->target_level - t->pan;
	t->per_channel_targets[1] = t->target_level;
    }
    else if (t->pan < 0)
    {
	t->per_channel_targets[0] = t->target_level;
	t->per_channel_targets[1] = t->target_level + t->pan;
    }
    
    return 0;
}

int sport_ioctl_setquantization(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct sport_track_level_s *tl = (struct sport_track_level_s *) arg;
    
    if (tl->track < 1 || tl->track > 8 || tl->level < 1 || tl->level > 128)
	return -EINVAL;

    t = &s->loop->tracks[tl->track - 1];
    t->quantized_steps = tl->level;

    return 0;
}

int sport_ioctl_trackfeedback(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct sport_track_level_s *tl = (struct sport_track_level_s *) arg;
    
    if (tl->track < 1 || tl->track > 8 || tl->level < 0 || tl->level > 100)
	return -EINVAL;

    t = &s->loop->tracks[tl->track - 1];

    if (tl->level == 100)
	t->feedback = 128;
    else
	t->feedback = (tl->level * 128) / 100;

    return 0;
}

int sport_ioctl_multiply(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    struct sport_track_level_s *tl = (struct sport_track_level_s *) arg;
    unsigned short from_block;
    unsigned short to_block;
    unsigned short from_pos;
    unsigned short to_pos;
    unsigned int pos;
    unsigned int n;
    unsigned int length;
    short failed = 0;
    short i;
    unsigned long flags;
    
    if (tl->track < 1 || tl->track > 8 || tl->level < 2 || tl->level > 8)
	return -EINVAL;

    t = &s->loop->tracks[tl->track - 1];

    spin_lock_irqsave(&sport_dma_lock, flags);
    length = t->length;
    to_block = length / LOOPER_BLOCK_SIZE;
    to_pos = length - (to_block * LOOPER_BLOCK_SIZE);
    spin_unlock_irqrestore(&sport_dma_lock, flags);

    for (i = 1; i < tl->level; i++)
    {
	from_block = 0;
	from_pos = 0;
	pos = 0;
    
	if (t->audio[to_block] == NULL)
	    t->audio[to_block] = (loop_audio_t *) looper_alloc();

	if (t->audio[to_block] == NULL)
	    failed = 1;

	/*
	 * Number of samples per block is not dependent on sample
	 * size, but if 16-bit audio, then we only need to copy half
	 * as much data and the offsets need to be divided by 2.
	 */
	while (pos < length && !failed)
	{
	    if (to_pos > from_pos)
		n = LOOPER_BLOCK_SIZE - to_pos;
	    else
		n = LOOPER_BLOCK_SIZE - from_pos;

	    if (length - pos < n)
		n = length - pos;

	    if (t->audio[from_block] != NULL)
	    {
		memcpy((char *) t->audio[to_block] + (to_pos >> AUDIO_OFFSET_SHIFT), 
		       (char *) t->audio[from_block] + (from_pos >> AUDIO_OFFSET_SHIFT), 
		       (n >> AUDIO_OFFSET_SHIFT));
	    }

	    from_pos += n;
	    if (from_pos >= LOOPER_BLOCK_SIZE)
	    {
		from_pos = 0;
		from_block++;
	    }
	
	    to_pos += n;
	    if (to_pos >= LOOPER_BLOCK_SIZE)
	    {
		to_pos = 0;
		to_block++;

		if (t->audio[to_block] == NULL)
		    t->audio[to_block] = (loop_audio_t *) looper_alloc();

		if (t->audio[to_block] == NULL)
		    failed = 1;
	    }
	
	    pos += n;
	}
    }
    
    if (!failed)
    {
	spin_lock_irqsave(&sport_dma_lock, flags);
	t->length = length * tl->level;
	spin_unlock_irqrestore(&sport_dma_lock, flags);
    }
    else
    {
	return -ENOMEM;
    }

    return 0;
}

int sport_ioctl_midi_clock(unsigned long arg)
{
    sport_info.midi_clk_detect = 1;
    return 0;
}

int sport_ioctl_reverse(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;

    if (arg < 1 || arg > 8)
	return -EINVAL;

    t = &s->loop->tracks[arg - 1];

    if (t->length == 0 || 
	s->loop->overdub_track == arg - 1 ||
	s->loop->replace_track == arg - 1)
    {
	return -EINVAL;
    }

    if (s->loop->record_track == arg - 1 || s->loop->bounce_track == arg - 1)
    {
	sport_end_record();
	t->options &= ~MUSIC_TRACK_OPTION_CUED;
    }

    if (t->options & MUSIC_TRACK_OPTION_PLAYING)
    {
	t->options |= MUSIC_TRACK_OPTION_REVERSEFADE;
	play_fade_stop(t);
    }
    else
	t->options ^= MUSIC_TRACK_OPTION_REVERSE;

    return 0;
}

int sport_ioctl_cue(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;

    if (arg < 1 || arg > 8)
	return -EINVAL;

    t = &s->loop->tracks[arg - 1];
    if ((t->options & MUSIC_TRACK_OPTION_RECORDING) ||
	(t->options & MUSIC_TRACK_OPTION_PLAYING) ||
	s->loop->record_track == arg - 1 ||
	s->loop->bounce_track == arg - 1 ||
	s->loop->replace_track == arg - 1 ||
	s->loop->overdub_track == arg - 1)
    {
	return -EINVAL;
    }

    t->options |= MUSIC_TRACK_OPTION_CUED;
    t->pos = 0;

    return 0;
}

int sport_ioctl_halfspeed(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;

    if (arg < 1 || arg > 8)
	return -EINVAL;

    t = &s->loop->tracks[arg - 1];

#if 0
    if (t->length == 0 || 
	(t->options & MUSIC_TRACK_OPTION_RECORDING) ||
	s->loop->record_track == arg - 1 ||
	s->loop->bounce_track == arg - 1 ||
	s->loop->replace_track == arg - 1)
    {
	return -EINVAL;
    }
#endif

    if (t->options & MUSIC_TRACK_OPTION_HALFSPEED)
    {
	t->options &= ~MUSIC_TRACK_OPTION_HALFSPEED;
	t->slow_cycle_inc *= 2;
    }
    else
    {
	t->options |= MUSIC_TRACK_OPTION_HALFSPEED;
	t->slow_cycle_inc /= 2;
    }

    return 0;
}

int sport_ioctl_setspeed(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    int speed;
    
    speed = (arg >> 8);
    arg &= 0xff;

    if (arg < 1 || arg > 8 || speed < 0 || speed > 100)
	return -EINVAL;

    t = &s->loop->tracks[arg - 1];

    if (t->options & MUSIC_TRACK_OPTION_HALFSPEED)
	t->slow_cycle_inc = 100 + speed;
    else
	t->slow_cycle_inc = 200 + 2 * speed;

    return 0;
}

int sport_ioctl_setvolumespeed(unsigned long arg)
{
    switch (arg)
    {
      case 0:
	sport_info.volume_change_threshold = (2L << 24);
	break;
      case 1:
	sport_info.volume_change_threshold = 100000;
	break;
      case 2:
	sport_info.volume_change_threshold = 50000;
	break;
      case 3:
	sport_info.volume_change_threshold = 5000;
	break;
    }

    return 0;
}

int sport_ioctl_scramble(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    int speed;
    int nblocks;
    int nchunks;
    int i;
    int ri;
    int b;
    int b1;
    int b2;
    short stmp;

    speed = (arg >> 8);
    arg &= 0xff;
    
    if (arg < 1 || arg > 8 || speed < 1 || speed > 64)
	return -EINVAL;

    t = &s->loop->tracks[arg - 1];
    if (t->length == 0 || 
	(t->options & MUSIC_TRACK_OPTION_RECORDING) ||
	s->loop->record_track == arg - 1 ||
	s->loop->bounce_track == arg - 1)
    {
	return -EINVAL;
    }

    if (t->options & MUSIC_TRACK_OPTION_SCRAMBLED)
    {
	t->options |= MUSIC_TRACK_OPTION_SCR_ENDING;
	play_fade_stop(t);
    }
    else
    {
	nblocks = t->length / LOOPER_BLOCK_SIZE;
	nchunks = (nblocks - 2) / speed;
	
	for (i = 0; i < nblocks + 1; i++)
	    t->scrambled[i] = (short) i;
	
	for (i = 0; i < nchunks; i++)
	{
	    ri = i + net_random() % (nchunks - i);

	    for (b = 0; b < speed; b++)
	    {
		b1 = i * speed + 1 + b;
		b2 = ri * speed + 1 + b;
		
		stmp = t->scrambled[b1];
		t->scrambled[b1] = t->scrambled[b2];
		t->scrambled[b2] = stmp;
	    }
	}
	
	t->options |= MUSIC_TRACK_OPTION_SCR_PENDING;
	play_fade_stop(t);
    }

    return 0;
}

int sport_ioctl_copy(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *d_t;
    struct music_track_s *s_t;
    unsigned long flags;
    short dtn;
    short stn;
    short b;

    dtn = (short) (arg >> 16) - 1;
    stn = (short) arg - 1;

    if (dtn < 0 || dtn >= 8 || 
	stn < 0 || stn >= 8 || 
	stn == dtn ||
	s->loop->record_track == stn ||
	s->loop->bounce_track == stn ||
	s->loop->overdub_track == stn ||
	s->loop->replace_track == stn ||
	s->loop->record_track == dtn ||
	s->loop->bounce_track == dtn ||
	s->loop->overdub_track == dtn ||
	s->loop->replace_track == dtn ||
	s->loop->tracks[stn].length == 0 ||
	s->loop->tracks[dtn].length > 0)
    {
	return -EINVAL;
    }

    d_t = &s->loop->tracks[dtn];
    s_t = &s->loop->tracks[stn];

    for (b = 0; s_t->audio[b] != NULL; b++)
    {
	if (d_t->audio[b] == NULL)
	{
	    spin_lock_irqsave(&sport_dma_lock, flags);

	    d_t->audio[b] = (loop_audio_t *) looper_alloc();

	    spin_unlock_irqrestore(&sport_dma_lock, flags);
	}
	
	if (d_t->audio[b] == NULL)
	{
	    // Failed - out of memory
	    b--;
	    while (b >= 0)
	    {
		looper_free((void *) d_t->audio[b]);
		d_t->audio[b] = NULL;
		b--;
	    }

	    return -EIO;
	}

	memcpy(d_t->audio[b], s_t->audio[b], (LOOPER_BLOCK_SIZE >> AUDIO_OFFSET_SHIFT));
    }

    d_t->length = s_t->length;
    d_t->original_length = s_t->original_length;
    d_t->virtual_start_offset = s_t->virtual_start_offset;
    d_t->target_level = s_t->target_level;
    d_t->pan = s_t->pan;
    d_t->feedback = s_t->feedback;
    d_t->slow_cycle_inc = s_t->slow_cycle_inc;
    d_t->quarternote_period = s_t->quarternote_period;
    d_t->midiclock_period = s_t->midiclock_period;
    d_t->midiclock_offset = s_t->midiclock_offset;
    d_t->quantized_steps = s_t->quantized_steps;
    d_t->replace_blend_direction = s_t->replace_blend_direction;
    d_t->replace_blend_level = s_t->replace_blend_level;
    memcpy(d_t->per_channel_targets, s_t->per_channel_targets, 
	   2 * sizeof(short));
    memcpy(d_t->scrambled, s_t->scrambled, LOOPER_MAX_BLOCKS * sizeof(short));

    spin_lock_irqsave(&sport_dma_lock, flags);

    d_t->options = s_t->options;
    d_t->pos = s_t->pos;
    d_t->fadeout = s_t->fadeout;
    d_t->slow_cycle = s_t->slow_cycle;
    d_t->midi_sync_count = s_t->midi_sync_count;
    memcpy(d_t->fadein_level, s_t->fadein_level, 2 * sizeof(short));
    memcpy(d_t->fadeout_level, s_t->fadeout_level, 2 * sizeof(short));
    memcpy(d_t->levels, s_t->levels, 2 * sizeof(short));
    memcpy(d_t->play_fade_direction, s_t->play_fade_direction, 
	   2 * sizeof(short));
    memcpy(d_t->play_fade_level, s_t->play_fade_level, 2 * sizeof(short));

    // Stop original track
    s_t->play_fade_direction[0] = 0;
    s_t->play_fade_direction[1] = 0;
    s_t->options &= ~(MUSIC_TRACK_OPTION_DELAYEDPLAY | 
		      MUSIC_TRACK_OPTION_PLAYING);

    spin_unlock_irqrestore(&sport_dma_lock, flags);

    return 0;
}

int sport_ioctl_undo(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    short i;
    loop_audio_t *lp;

    if (s->loop->undo_track < 0)
	return -EINVAL;
    
    t = &s->loop->tracks[s->loop->undo_track];

    for (i = 0; 
	 (i < LOOPER_MAX_BLOCKS && 
	  s->loop->undo_audio[i] != NULL &&
	  t->audio[i] != NULL); 
	 i++)
    {
	lp = t->audio[i];
	t->audio[i] = s->loop->undo_audio[i];
	s->loop->undo_audio[i] = lp;
    }

    return 0;
}

int sport_ioctl_overdub(unsigned long arg)
{
    struct sport_s *s = &sport_info;
    struct music_track_s *t;
    unsigned long flags;
    loop_audio_t *lap;
    long starting_pos;
    long l;
    short starting_block = 0;
    long offset;
    short block = 0;
    short i;
    short undo_fail = 0;
    short was_recording = 0;

    if ((s->loop->record_track >= 0 && s->loop->record_track != arg - 1) || 
	(s->loop->bounce_track >= 0 && s->loop->bounce_track != arg - 1) || 
	s->loop->overdub_track >= 0 ||
	s->loop->replace_track >= 0)
    {
	return -EINVAL;
    }

    if (s->loop->record_track >= 0 || s->loop->bounce_track >= 0)
	was_recording = 1;

    if (arg >= 1 && arg <= 8)
    {
	t = &s->loop->tracks[arg - 1];

	if (t->length == 0)
	    return -EINVAL;

	// Create first 2 blocks of undo if track not too long
	if (t->length <= 10485760) /* 30 seconds */
	{
	    undo_fail = 0;
	    s->loop->undo_track = -2;
	    
	    spin_lock_irqsave(&sport_dma_lock, flags);
	    starting_pos = t->pos;
	    spin_unlock_irqrestore(&sport_dma_lock, flags);

	    starting_block = starting_pos / LOOPER_BLOCK_SIZE;
	    if (t->audio[starting_block] == NULL)
		starting_block = 0;

	    block = starting_block;

	    for (i = 0; i < 2 && !undo_fail; i++)
	    {
		if (s->loop->undo_audio[block] == NULL)
		{
		    undo_fail = 1;
		}
		else
		{
		    memcpy(s->loop->undo_audio[block], t->audio[block], 
			   (LOOPER_BLOCK_SIZE >> AUDIO_OFFSET_SHIFT));
		}
		
		block++;
		if (t->audio[block] == NULL)
		    block = 0;
	    }
	}
	else
	    undo_fail = 1;

	// Start overdub
	t->options &= ~MUSIC_TRACK_OPTION_CUED;
	t->options |= MUSIC_TRACK_OPTION_PLAYING;
	sport_end_record();
	s->loop->overdub_track = arg - 1;

	// Finish creating undo information
	if (!undo_fail)
	{
	    while (block != starting_block && !undo_fail)
	    {
		if (s->loop->undo_audio[block] == NULL)
		{
		    undo_fail = 1;
		}
		else
		{
		    memcpy(s->loop->undo_audio[block], t->audio[block], 
			   (LOOPER_BLOCK_SIZE >> AUDIO_OFFSET_SHIFT));
		}
		
		block++;
		if (t->audio[block] == NULL)
		    block = 0;
	    }

	    if (!undo_fail)
	    {
		// If we were in record mode before the switch to overdub,
		// then we need to fix up the end of the undo audio to avoid
		// a nasty click.
		for (l = t->length - 4; l > 0; l -= 8)
		{
		    block = l / LOOPER_BLOCK_SIZE;
		    offset = l - (block * LOOPER_BLOCK_SIZE);

		    lap = s->loop->undo_audio[block] + (offset >> 2);
		    if (*lap > ZERO_THRESHOLD || *lap < -ZERO_THRESHOLD)
			*lap = 0;
		    else
			break;
		}
		for (l = t->length - 8; l > 0; l -= 8)
		{
		    block = l / LOOPER_BLOCK_SIZE;
		    offset = l - (block * LOOPER_BLOCK_SIZE);

		    lap = s->loop->undo_audio[block] + (offset >> 2);
		    if (*lap > ZERO_THRESHOLD || *lap < -ZERO_THRESHOLD)
			*lap = 0;
		    else
			break;
		}

		// Mark the undo audio as valid
		s->loop->undo_track = arg - 1;
	    }
	}

	if (undo_fail)
	    s->loop->undo_track = -1;
    }
    else
	return -EINVAL;

    return 0;
}

int sport_ioctl(struct inode *inode, struct file *filp, 
		uint cmd, unsigned long arg)
{
    int rv = -EINVAL;
    int n;
    
    svm = sport_volume_mult;

    switch (cmd)
    {
      case CMD_SPORT_LOOPSTATUS:
	rv = sport_ioctl_loopstatus(arg);
	break;
      case CMD_SPORT_SETMASTER:
	rv = sport_ioctl_setmaster(arg);
	break;
      case CMD_SPORT_STOP:
	rv = sport_ioctl_delayedstop(arg);
	break;
      case CMD_SPORT_PLAY:
	rv = sport_ioctl_delayedplay(arg);
	break;
      case CMD_SPORT_STOPNOW:
	rv = sport_ioctl_immedstop(arg);
	break;
      case CMD_SPORT_PLAYNOW:
	rv = sport_ioctl_immedplay(arg);
	break;
      case CMD_SPORT_RECORD:
	rv = sport_ioctl_record(arg);
	break;
      case CMD_MIDISYNC_RECORD:
	rv = sport_ioctl_midisync_record(arg);
	break;
      case CMD_SYNC_RECORD:
	rv = sport_ioctl_sync_record(arg);
	break;
      case CMD_SPORT_BOUNCE:
	rv = sport_ioctl_bounce(arg);
	break;
      case CMD_SPORT_OVERDUB:
	rv = sport_ioctl_overdub(arg);
	break;
      case CMD_SPORT_REPLACE:
	rv = sport_ioctl_replace(arg);
	break;
      case CMD_SPORT_QREPLACE:
	rv = sport_ioctl_quantizedreplace(arg);
	break;
      case CMD_SPORT_ERASE:
	rv = sport_ioctl_erase(arg);
	break;
      case CMD_SPORT_REVERSE:
	rv = sport_ioctl_reverse(arg);
	break;
      case CMD_SPORT_DOUBLESPEED:
	break;
      case CMD_SPORT_HALFSPEED:
	rv = sport_ioctl_halfspeed(arg);
	break;
      case CMD_SPORT_SETSPEED:
	rv = sport_ioctl_setspeed(arg);
	break;
      case CMD_SPORT_TRACKLEVEL:
	rv = sport_ioctl_tracklevel(arg);
	break;
      case CMD_SPORT_TRACKFEEDBACK:
	rv = sport_ioctl_trackfeedback(arg);
	break;
      case CMD_SPORT_NOISEGATELVL:
	if (arg < 0)
	    return -EINVAL;
	sport_info.noise_gate_level = arg;
	rv = 0;
	break;
      case CMD_SPORT_NOISEGATETIME:
	if (arg < 1 || arg > 200)
	    return -EINVAL;
	sport_info.noise_gate_delay = (short) arg * 50;
	rv = 0;
	break;
      case CMD_SPORT_MASTERVOLUME:
	n = arg;
	
	if (n > 8 || n < -8)
	    return -EINVAL;
	sport_info.master_volume = (short) n;
	rv = 0;
	break;
      case CMD_SPORT_MULTIPLY:
	rv = sport_ioctl_multiply(arg);
	break;
      case CMD_MIDI_CLOCK:
	rv = sport_ioctl_midi_clock(arg);
	break;
      case CMD_SPORT_WETONLY:
	if (arg)
	    sport_info.wet_only = 1;
	else
	    sport_info.wet_only = 0;
	rv = 0;
	break;
      case CMD_SPORT_RECORDLEVELTRIGGER:
	if (arg > 0 && arg < 24)
	    sport_info.record_level_threshold = (1L << arg);
	else
	    sport_info.record_level_threshold = 0;

	DDEBUG(("RECORD LEVEL THRESHOLD = %lx (%ld)\n",
	       sport_info.record_level_threshold,
	       sport_info.record_level_threshold));

	rv = 0;
	break;
	
      case CMD_SPORT_MIDISYNCOUTENABLE:
	if (arg)
	    sport_info.midi_sync_out_enabled = 1;
	else
	    sport_info.midi_sync_out_enabled = 0;
	rv = 0;
	break;
      case CMD_MIDISYNC_BPMEASURE:
	if (arg > 0 && arg < 300)
	    sport_info.beats_per_measure = (short) arg;
	rv = 0;
	break;
      case CMD_SPORT_CUE:
	rv = sport_ioctl_cue(arg);
	break;
      case CMD_GET_MIDI_CLOCK_ADDR:
	{
	    void *p = (void *) sport_ioctl_midi_clock;
	    copy_to_user((void *) arg, &p, sizeof(p));
	    DDEBUG(("sport_ioctl_midi_clock = %lx\n", (unsigned long) p));

	    p = (void *) sport_check_clock_out;
	    copy_to_user((void *) (arg + sizeof(void *)), &p, sizeof(p));
	    DDEBUG(("sport_check_clock_out = %lx\n", (unsigned long) p));
	}
	rv = 0;
	break;
      case CMD_SPORT_ASSIGNAUX1:
	sport_info.aux1_mask = (short) arg;
	sport_info.aux2_mask &= (short) ~arg;
	rv = 0;
	break;
	
      case CMD_SPORT_ASSIGNAUX2:
	sport_info.aux2_mask = (short) arg;
	sport_info.aux1_mask &= (short) ~arg;
	rv = 0;
	break;
	
      case CMD_SPORT_SCRAMBLE:
	rv = sport_ioctl_scramble(arg);
	break;

      case CMD_SPORT_VOLUMESPEED:
	rv = sport_ioctl_setvolumespeed(arg);
	break;

      case CMD_SPORT_PAN:
	rv = sport_ioctl_pan(arg);
	break;

      case CMD_SPORT_SETQUANT:
	rv = sport_ioctl_setquantization(arg);
	break;

      case CMD_SPORT_COPY:
	rv = sport_ioctl_copy(arg);
	break;

      case CMD_SPORT_UNDO:
	rv = sport_ioctl_undo(arg);
	break;
    }
    
    return rv;
}

static long noise_gate(long sample_in, struct music_loop_s *loop, 
		       short channel)
{
    if (channel && sample_counter > 0)
	sample_counter--;
    
    if (sample_in > 0)
    {
	if (sample_in > sport_info.max_pos_value)
	    sport_info.max_pos_value = sample_in;
	if (sample_in < sport_info.min_pos_value)
	    sport_info.min_pos_value = sample_in;
    }
	    
    if ((sample_in > 0 && sample_in < sport_info.noise_gate_level) ||
	(sample_in < 0 && sample_in > -sport_info.noise_gate_level))
    {
	if (sport_info.noise_gate_time[channel] >
	    sport_info.noise_gate_delay)
	{
	    sport_info.noise_gate_release[channel] = 0;
	    return 0;
	}
	else
	    sport_info.noise_gate_time[channel]++;
    }
    else
    {
	if (sport_info.noise_gate_release[channel] >
	    (sport_info.noise_gate_delay >> 3))
	{
	    sport_info.noise_gate_time[channel] = 0;
	}
	else
	{
	    sport_info.noise_gate_release[channel]++;
	    return 0;
	}
    }

    return sample_in;
}

static long fade_in(long sample_in, struct music_loop_s *loop, 
		    struct music_track_s *track, int initialize, short channel)
{
    if (initialize)
    {
	track->fadein_level[0] = -128;
	track->fadein_level[1] = -128;
	track->fadein_samples[0] = 0;
	track->fadein_samples[1] = 0;
    }

    if (track->fadein_level[channel] < 0)
    {
#ifndef USE_ZERO_CROSSING_DETECTOR
	track->fadein_level[channel] += 1;
	sample_in = apply_volume(sample_in, track->fadein_level[channel]);
#else
	if (track->fadein_samples[channel] == 0)
	{
	    if (sample_in < 0)
		track->fadein_samples[channel] = sample_in;
	    sample_in = 0;
	}
	else if (sample_in > 0)
	{
	    track->fadein_level[channel] = 0;
	}
	else
	    sample_in = 0;
#endif
    }

    return sample_in;
}

static long fade_out(long sample_in, struct music_loop_s *loop, 
		    struct music_track_s *track, int initialize, short channel)
{
    if (initialize)
    {
	track->fadeout = 1;
	
	track->fadeout_level[0] = 0;
	track->fadeout_level[1] = 0;

	return sample_in;
    }

#ifndef USE_ZERO_CROSSING_DETECTOR
    if (track->fadeout_level[channel] > -129)
    {
	track->fadeout_level[channel]--;
	sample_in = apply_volume(sample_in, track->fadeout_level[channel]);
    }
    else
	sample_in = 0;

    if (track->fadeout > 0 && 
	track->fadeout_level[0] <= -129 && 
	track->fadeout_level[1] <= -129)
    {
	track->fadeout = 0;
	sample_in = 0;
    }
#else
    if (track->fadeout_level[channel] > -129)
    {
	if (sample_in > -256 && sample_in < 256 && 
	    track->fadeout_level[1 - channel] <= track->fadeout_level[channel])
	{
	    track->fadeout_level[channel] -= 10;
	}

	sample_in = apply_volume(sample_in, track->fadeout_level[channel]);
    }
    else
	sample_in = 0;

    if (track->fadeout > 0 && 
	track->fadeout_level[0] <= -129 && 
	track->fadeout_level[1] <= -129)
    {
	track->fadeout = 0;
	sample_in = 0;
    }
#endif

    return sample_in;
}

static void sport_end_record()
{
    struct sport_s *s = &sport_info;
    struct music_loop_s *loop = s->loop;

    /* Turn off recording */
    loop->record_track = -1;
    loop->bounce_track = -1;
}

static void record_track(long sample_in, struct music_loop_s *loop, 
			 struct music_track_s *track, short channel)
{
    short block;
    short offset;

    if (!(track->options & MUSIC_TRACK_OPTION_RECORDING))
    {
	if (sport_info.record_level_threshold > 0 && 
	    ((sample_in >= 0 && 
	      sample_in < sport_info.record_level_threshold) ||
	     (sample_in < 0 && 
	      sample_in > -sport_info.record_level_threshold) ||
	     channel == 1))
	{
	    return;
	}

	track->options |= MUSIC_TRACK_OPTION_RECORDING;
	sample_in = fade_in(sample_in, loop, track, 1, channel);
    }
    else
	sample_in = fade_in(sample_in, loop, track, 0, channel);
    
    block = track->pos / LOOPER_BLOCK_SIZE;
    offset = track->pos - (block * LOOPER_BLOCK_SIZE);

    if (track->audio[block] == NULL)
	track->audio[block] = (loop_audio_t *) looper_alloc();
    if (track->audio[block] == NULL)
    {
	loop->record_track = -1;
	loop->bounce_track = -1;
    }
    else
    {
	(track->audio[block])[offset >> 2] = (sample_in >> AUDIO_DATA_SHIFT);
	if ((track->options & MUSIC_TRACK_OPTION_SYNC))
	{
	    if (track->length <= track->pos)
		track->length += track->original_length;
	}
	else if (!(track->options & MUSIC_TRACK_OPTION_MIDISYNC))
	{
	    if (track->length <= track->pos)
	    {
		track->length += 4;
		track->original_length += 4;
	    }
	}
	else if (track->length <= track->pos)
	{
	    track->length += track->original_length;
	}
    }
}

static void overdub_track(long sample_in, struct music_loop_s *loop, 
		     struct music_track_s *track, short channel)
{
    short block;
    short offset;
    
    if (track->fadeout > 0)
    {
	sample_in = fade_out(sample_in, loop, track, 0, channel);
    }
    else if (!(track->options & MUSIC_TRACK_OPTION_RECORDING))
    {
	track->options |= MUSIC_TRACK_OPTION_RECORDING;
	sample_in = fade_in(sample_in, loop, track, 1, channel);
    }
    else
	sample_in = fade_in(sample_in, loop, track, 0, channel);

    block = track->pos / LOOPER_BLOCK_SIZE;
    offset = track->pos - (block * LOOPER_BLOCK_SIZE);

    if (track->audio[block] == NULL)
    {
	track->audio[block] = (loop_audio_t *) looper_alloc();
    }
    
    if (track->audio[block] != NULL)
    {
	(track->audio[block])[offset >> 2] += (sample_in >> AUDIO_DATA_SHIFT);
    }
}

static long replace_track(long sample_in, struct music_loop_s *loop, 
			  struct music_track_s *track, short channel)
{
    short block;
    short offset;
    long track_sample = 0;
    long sample_out = 0;
    short blend_level;
    
    block = track->pos / LOOPER_BLOCK_SIZE;
    offset = track->pos - (block * LOOPER_BLOCK_SIZE);

    if (track->audio[block] == NULL)
	return 0;

    if (track->replace_blend_direction != 0)
    {
	blend_level = track->replace_blend_level >> 3;

	track_sample = ((track->audio[block])[offset >> 2]) << AUDIO_DATA_SHIFT;
	sample_out = track_sample;

	track_sample = apply_volume(track_sample, blend_level);
	
	sample_in = apply_volume(sample_in, -blend_level - 128) + track_sample;

	track->replace_blend_level += track->replace_blend_direction;
	if (track->replace_blend_level > 0 || track->replace_blend_level < -1023)
	{
	    track->replace_blend_direction = 0;
	}
    }
    else if (track->options & MUSIC_TRACK_OPTION_REPLACEPLUS)
	sample_out = ((track->audio[block])[offset >> 2]) << AUDIO_DATA_SHIFT;

    (track->audio[block])[offset >> 2] = (sample_in >> AUDIO_DATA_SHIFT);

    if (track->options & MUSIC_TRACK_OPTION_REPLACEPLUS)
	return sample_out;
    else
	return track_sample;
}

static inline long play_track(struct music_loop_s *loop, struct music_track_s *track, 
		       short channel)
{
    long threshold = sport_info.volume_change_threshold;
    unsigned int block;
    unsigned int offset;
    long track_sample;
    int blend_level;
    int track_level;

    block = track->pos / LOOPER_BLOCK_SIZE;
    offset = track->pos - (block * LOOPER_BLOCK_SIZE);

    if (track->audio[block] != NULL)
    {
	track_sample = ((int32_t) (track->audio[block])[offset >> 2]) << AUDIO_DATA_SHIFT;

	/* Handle fading if feedback is less than 100% */
	if (track->feedback < 128)
	{
	    (track->audio[block])[offset >> 2] = 
		(track_sample * track->feedback) >> (7 + AUDIO_DATA_SHIFT);
	}
    }
    else
	track_sample = 0;

    // Level adjustments
    track_level = track->levels[channel];

    if (track_level != track->per_channel_targets[channel])
    {
	if (track_sample > -ZERO_THRESHOLD && track_sample < ZERO_THRESHOLD)
	{
	    track_level = track->per_channel_targets[channel];
	    track->levels[channel] = track_level;
	}
    }
    
    if (track->play_fade_direction[channel] != 0)
    {
	/* Compute current blend level */
	blend_level = track->play_fade_level[channel];

	/* Adjust blend level if within the allowed range */
	if (blend_level <= 0 && blend_level >= -511 &&
	    ((track_sample < 0 && track_sample > -threshold) ||
	     (track_sample >= 0 && track_sample < threshold)))
	{
	    track->play_fade_level[channel] += 
		track->play_fade_direction[channel];
	}
	
	/* Apply blend level to track sample */
	track_level += blend_level;

	if ((track->play_fade_level[0] > 0 || 
	     track->play_fade_level[0] < -511) &&
	    (track->play_fade_level[1] > 0 || 
	     track->play_fade_level[1] < -511))
	{
	    if (track->options & MUSIC_TRACK_OPTION_REVERSEFADE)
	    {
		track->options &= ~MUSIC_TRACK_OPTION_REVERSEFADE;
		track->options ^= MUSIC_TRACK_OPTION_REVERSE;
		
		track->play_fade_level[0] = -511;
		track->play_fade_level[1] = -511;
		track->play_fade_direction[0] = 1;
		track->play_fade_direction[1] = 1;
	    }
	    else
	    {
		track->play_fade_direction[0] = 0;
		track->play_fade_direction[1] = 0;
	    }
	}
    }

    return apply_volume_512steps(track_sample, track_level);
}

static void sport_trigger_cued(short stopped_track)
{
    struct music_track_s *track = NULL;
    struct sport_s *s = &sport_info;
    short track_num;
    short cue_record = -1;
    short n_cued = 0;

    track = &s->loop->tracks[0];
    for (track_num = 0; track_num < 8; track_num++, track++)
    {
	if (track->options & MUSIC_TRACK_OPTION_CUED)
	{
	    n_cued++;
	    
	    track->options &= ~MUSIC_TRACK_OPTION_CUED;

	    if (track->length != 0)
	    {
		track->options |= MUSIC_TRACK_OPTION_PLAYING;
		track->pos = track->virtual_start_offset;
		play_fade_start(track);
	    }
	    else if (s->loop->record_track < 0 &&
		     s->loop->bounce_track < 0 &&
		     s->loop->overdub_track < 0)
	    {
		cue_record = track_num;
	    }
	}
    }

    if (cue_record >= 0 && n_cued == 1)
    {
	track = &s->loop->tracks[cue_record];
	track->options &= ~MUSIC_TRACK_OPTION_REVERSE;
	track->options |= MUSIC_TRACK_OPTION_PLAYING;
	track->length = 0;
	track->original_length = 0;
	track->virtual_start_offset = 0;
	track->pos = 0;

	if (s->loop->master_track < 0)
	    s->loop->master_track = cue_record;

	if (stopped_track > 0 && 
	    (s->loop->tracks[stopped_track-1].options & 
	     MUSIC_TRACK_OPTION_MIDISYNC))
	{
	    track->options |= MUSIC_TRACK_OPTION_MIDISYNC;

	    track->quarternote_period = 
		(sport_avg_midi_counter() * 24) >> (MIDI_FRACTION_BITS + 1);
	    track->midi_sync_count = sport_info.cur_midi_counter;
	    track->midiclock_period = sport_avg_midi_counter();
	    track->midiclock_offset = sport_info.cur_midi_counter;

	    track->virtual_start_offset = 0;

	    track->original_length = ((long) (SPORT_DMA_BYTES * 
					  sport_info.beats_per_measure) * 
				  track->quarternote_period);
	    track->length = track->original_length;
	    track->length_check_point = track->original_length / 4;
	}

	s->loop->record_track = cue_record;
    }
}

void play_fade_start(struct music_track_s *track)
{
    unsigned long flags;

    spin_lock_irqsave(&sport_dma_lock, flags);

    track->play_fade_direction[0] = 0;
    track->play_fade_direction[1] = 0;
    track->play_fade_level[0] = -511;
    track->play_fade_level[1] = -511;
    track->play_fade_direction[0] = 1;
    track->play_fade_direction[1] = 1;

    spin_unlock_irqrestore(&sport_dma_lock, flags);
}

void play_fade_stop(struct music_track_s *track)
{
    unsigned long flags;

    spin_lock_irqsave(&sport_dma_lock, flags);

    track->play_fade_direction[0] = 0;
    track->play_fade_direction[1] = 0;
    track->play_fade_level[0] = 0;
    track->play_fade_level[1] = 0;
    track->play_fade_direction[0] = -1;
    track->play_fade_direction[1] = -1;

    spin_unlock_irqrestore(&sport_dma_lock, flags);
}

void sport_stop_bouncing_tracks(void)
{
    struct sport_s *s = &sport_info;
    short i;
    short stopped_track = 0;
    
    for (i = 0; i < 8; i++)
    {
	if (s->loop->tracks[i].options & MUSIC_TRACK_OPTION_BOUNCEFROM)
	{
	    if (s->loop->tracks[i].options & MUSIC_TRACK_OPTION_PLAYING)
	    {
		if (stopped_track == 0)
		    stopped_track = i + 1;
		
		play_fade_stop(&s->loop->tracks[i]);
	    }
	    
	    s->loop->tracks[i].options &= ~MUSIC_TRACK_OPTION_PLAYING;
	    s->loop->tracks[i].options &= ~MUSIC_TRACK_OPTION_CUED;
	}
    }

    sport_trigger_cued(stopped_track);
}

void sport_fade_loop_end(struct music_track_s *track)
{
    loop_audio_t *audiop;
    long pos;
    short block;
    int offset;
    short level;
    
    pos = track->length - 4096;
    block = pos / LOOPER_BLOCK_SIZE;
    offset = pos - (block * LOOPER_BLOCK_SIZE);
    
    if (pos < 0)
	return;
    
    for (level = 0; pos < track->length; level--, pos += 4)
    {
	audiop = &(track->audio[block])[offset >> 2];
	*audiop = apply_volume(*audiop, (level >> 3));

	offset += 4;
	if (offset >= LOOPER_BLOCK_SIZE)
	{
	    offset = 0;
	    block++;
	}
    }
}

static irqreturn_t sport_error_handler(int irq, void *dev_id, 
				       struct pt_regs *regs)
{
    *pSPORT0_STAT = 0x0007;
    *pSPORT1_STAT = 0x0007;

    return IRQ_HANDLED;
}

static irqreturn_t sport_dma_rx_handler(int irq, void *dev_id, 
					struct pt_regs *regs)
{
    volatile long *efx_bufp;
    volatile long *aux_bufp;
    volatile long sample_in;
    volatile long sample_out;
    volatile long aux_out;
    volatile long aux2_out;
    long bounce_in;
    long sample;
    
    struct music_loop_s *loop;
    struct music_track_s *track = NULL;
    unsigned long flags;
    unsigned long desc_ptr;
    short rx_bufn;
    short fx_bufn;
    unsigned int status;
    short i;
    short j;
    short track_num;
    short process_interrupt = 1;
    short pos_adjust;

#if 0
    *pFIO_FLAG_S = 0x0010;
#endif

    looper_time_stamps[looper_time_stamp_idx] = *pTCOUNT;
    looper_time_stamp_idx = (looper_time_stamp_idx + 1) & (NUM_LOOPER_TIME_STAMPS - 1);
    spin_lock_irqsave(&sport_dma_lock, flags);

    svm = sport_volume_mult;

    status = get_dma_curr_irqstat(CH_SPORT0_RX);
    clear_dma_irqstat(CH_SPORT0_RX);
    desc_ptr = (unsigned long) *pDMA1_CURR_DESC_PTR;

    if ((desc_ptr & 0x7))
    {
	process_interrupt = 0;
    }

    if (process_interrupt)
    {
	loop = sport_info.loop;

	desc_ptr -= 8;
	fx_bufn = sport_info.next_fx_bufn;
	rx_bufn = (desc_ptr - (unsigned long) sport_rx_sg) / SPORT_SG_BYTES;
	if (rx_bufn < 0 || rx_bufn >= SPORT_DMA_BUFS)
	    rx_bufn = fx_bufn;

	if (sport_info.midi_clk_detect)
	{
	    sport_info.saved_midi_counters[sport_info.saved_midi_counters_idx++] = 
		sport_info.cur_midi_counter;
	    sport_info.saved_midi_counters_idx &= (NUM_SAVED_MIDI_COUNTERS - 1);
	    sport_info.cur_midi_counter = 0;
	}
	while (loop && fx_bufn != rx_bufn)
	{
	    /* Update midi clock */
	    sport_info.cur_midi_counter += (2 << MIDI_FRACTION_BITS);

	    track = &loop->tracks[0];
	    for (track_num = 0; track_num < 8; track_num++, track++)
	    {
		long error;
		
		if (track->options & MUSIC_TRACK_OPTION_MIDISYNC)
		{
		    if (track->midi_sync_count >= track->midiclock_period)
			track->midi_sync_count -= track->midiclock_period;

		    if (sport_info.midi_clk_detect)
		    {
			if (track->midi_sync_count > (track->midiclock_period >> 1))
			    error = track->midi_sync_count - track->midiclock_period;
			else
			    error = track->midi_sync_count;

			track->midi_errors[track->midi_errors_idx++] = error;
			track->midi_errors_idx &= 3;

			error = (track->midi_errors[0] + 
				 track->midi_errors[1] + 
				 track->midi_errors[2] +
				 track->midi_errors[3] + 
				 track->midi_errors[4] +
				 track->midi_errors[5] + 
				 track->midi_errors[6] +
				 track->midi_errors[7] + 
				 track->midi_errors[8] +
				 track->midi_errors[9] + 
				 track->midi_errors[10] +
				 track->midi_errors[11] + 
				 track->midi_errors[12] +
				 track->midi_errors[13] + 
				 track->midi_errors[14] +
				 track->midi_errors[15]) >> 4;

			if (track->pos >= track->length_check_point)
			{
//			    track->midiclock_offset -= error;

			    track->length_check_point = 0xffffffff;
			}
			else if (track_num != loop->record_track &&
				 track_num != loop->bounce_track &&
				 track_num != loop->overdub_track &&
				 track_num != loop->replace_track &&
				 track->fadeout == 0 &&
				 error != 0)
			{
			    track->midi_pos_adjust_num = error < 0 ? -error : error;
			    track->midi_pos_adjust_num >>= MIDI_FRACTION_BITS;
			    if (track->midi_pos_adjust_num > 5)
				track->midi_pos_adjust_num = 5;
			    
			    track->midi_pos_adjust = -error;
			    track->midi_pos_adjust_timer = 0;
			}
			if (track_num == 0 && looper_midi_clock_sample_idx < 1000)
			{
			    looper_midi_clock_samples[looper_midi_clock_sample_idx++] = error;
			}
		    }
		}
	    }

	    sport_info.midi_clk_detect = 0;

	    efx_bufp = sport_dma_buf + (fx_bufn * SPORT_DMA_LONGS);
	    aux_bufp = sport1_dma_buf + (fx_bufn * SPORT_DMA_LONGS * 2);

	    for (i = 0; i < SPORT_DMA_LONGS; i++)
	    {
		sample_in = efx_bufp[i];
		sample_in = noise_gate(sample_in, loop, (i & 1));

		if (!sport_info.wet_only)
		    sample_out = sample_in;
		else
		    sample_out = 0;

		aux_out = 0;
		aux2_out = 0;

		if (loop->bounce_options & MUSIC_BOUNCE_OPTION_MIXINPUT)
		    bounce_in = sample_in;
		else
		    bounce_in = 0;

		/*
		 * Cycle through all of the tracks.
		 */
		for (j = 0; j <= 8; j++)
		{
		    /*
		     * If we are bouncing to a track, then we need to process
		     * that track last.
		     */
		    if (j < 8)
		    {
			if (j == loop->bounce_track)
			    continue;
			
			track_num = j;
		    }
		    else
		    {
			track_num = loop->bounce_track;
			sample_in = bounce_in;

			if (track_num < 0)
			    continue;
		    }
		    
		    track = &loop->tracks[track_num];

		    /*
		     * Make certain that we keep left and right matched up.
		     */
		    if ((i & 1) != ((track->pos >> 2) & 1))
			continue;

		    /*
		     * If we aren't recording or bouncing into this track,
		     * then we need handle crossing the end of the loop with
		     * position marker.
		     */
		    if ((track->pos >= track->length || track->pos < 0) &&
			track_num != loop->record_track &&
			track_num != loop->bounce_track)
		    {
			// Reset MIDI counter to known offset at each loop start
			track->midi_sync_count = track->midiclock_offset; //xxxxx

#if 0
			long error = (track->midi_errors[0] + 
				      track->midi_errors[1] + 
				      track->midi_errors[2] + 
				      track->midi_errors[3]) >> (2 + MIDI_FRACTION_BITS);

			track->midi_pos_adjust_num = error < 0 ? -error : error;
			track->midi_pos_adjust = -error;
			track->midi_pos_adjust_timer = 0;
#endif
			
			if (track->pos < 0)
			    track->pos += track->length;
			else if (track->pos >= track->length)
			    track->pos = 0;
		    }

		    /*
		     * Have we been requested to end bouncing and is this the
		     * track we are bouncing in to?
		     */
		    else if (track_num == loop->bounce_track &&
			     (loop->bounce_options & 
			      MUSIC_BOUNCE_OPTION_ENDRECORD) &&
			     track->pos >= track->length)
		    {
			track->options &= ~MUSIC_TRACK_OPTION_RECORDING;
			track->options |= MUSIC_TRACK_OPTION_BOUNCEFADE;
			loop->bounce_track = -1;
			track->pos = 0;

			/* Stop all tracks that contributed to this bounce */
			sport_stop_bouncing_tracks();
		    }

		    /*
		     * Make certain virtual_start_offset is inside the loop.
		     *
		     * WHY ARE WE DOING THIS HERE? Shouldn't we do this when we
		     * set virtual_start_offset?
		     *
		     * xxxxx <- marker that I use to search for things to fix
		     */
		    if (track->virtual_start_offset < 0 &&
			track_num != loop->record_track &&
			track_num != loop->bounce_track)
		    {
			track->virtual_start_offset += track->length;
		    }

		    /*
		     * If we are at the virtual loop beginning, then we need to
		     * see if there are any delayed flags that need to be processed.
		     * Currently we have only delayed start and delayed stop.
		     */
		    if (track->pos == track->virtual_start_offset &&
			track_num != loop->record_track &&
			track_num != loop->bounce_track)
		    {
			/* Check for delayed flags to change status */
			if (track->options & MUSIC_TRACK_OPTION_DELAYEDPLAY)
			{
			    track->options &= ~MUSIC_TRACK_OPTION_DELAYEDPLAY;
			    track->options |= MUSIC_TRACK_OPTION_PLAYING;
			    play_fade_start(track);
			}
			else if (track->options & 
				 MUSIC_TRACK_OPTION_DELAYEDSTOP)
			{
			    track->options &= ~MUSIC_TRACK_OPTION_DELAYEDSTOP;
			    track->options &= ~MUSIC_TRACK_OPTION_PLAYING;
			    play_fade_stop(track);
			    sport_trigger_cued(track_num + 1);
			}
		    }

		    /*
		     * Is it time to stop recording to this track?
		     */
		    if ((track->options & MUSIC_TRACK_OPTION_RECORDING) &&
			loop->record_track != track_num && 
			loop->bounce_track != track_num &&
			loop->overdub_track != track_num)
		    {
			track->options &= ~MUSIC_TRACK_OPTION_RECORDING;
			fade_out(0, loop, track, 1, 0);
		    }

		    /*
		     * If we are recording or bouncing to this track, then
		     * this is where we do the audio processing for record.
		     */
		    if (track_num == loop->record_track ||
			track_num == loop->bounce_track)
		    {
			record_track(sample_in, loop, track, (short) (i & 1));
		    }

		    /*
		     * Are we replacing audio in this track?
		     */
		    else if (track_num == loop->replace_track ||
			     track->replace_blend_direction != 0)
		    {
			sample = replace_track(sample_in, loop, track,
					       (short) (i & 1));

			if (sport_info.aux1_mask & (1 << track_num))
			    aux_out += sample;
			else if (sport_info.aux2_mask & (1 << track_num))
			    aux2_out += sample;
			else
			    sample_out += sample;
		    }
		    
		    /*
		     * Not recording or replacing to this track.  This is playback
		     * and possibly overdub processing.
		     */
		    else
		    {
			/*
			 * Retrigger flag set and stopped?
			 */
			if (track->length != 0 &&
			    !(track->options & MUSIC_TRACK_OPTION_PLAYING) &&
			    track->play_fade_direction[(i & 1)] == 0 &&
			    (track->options & MUSIC_TRACK_OPTION_RETRIGGER))
			{
			    track->pos = track->virtual_start_offset;

			    if (track_num == 0)
			    {
				sport_info.clock_segment = 0;
				sport_info.next_clock_pos = 0;
			    }

			    track->options &=~ MUSIC_TRACK_OPTION_RETRIGGER;
			    track->options |= MUSIC_TRACK_OPTION_PLAYING;
			    play_fade_start(track);
			}
			
			sample = play_track(loop, track, (short) (i & 1));

			if (track->length != 0 &&
			    ((track->options & MUSIC_TRACK_OPTION_PLAYING) ||
			     track->play_fade_direction[(i & 1)] != 0))
			{
			    if (sport_info.aux1_mask & (1 << track_num))
				aux_out += sample;
			    else if (sport_info.aux2_mask & (1 << track_num))
				aux2_out += sample;
			    else
				sample_out += sample;

			    if (track->options & MUSIC_TRACK_OPTION_BOUNCEFROM)
				bounce_in += sample;
			}
			
			if (track_num == loop->overdub_track ||
			    track->fadeout > 0)
			{
			    overdub_track(sample_in, loop, track,
					  (short) (i & 1));
			}
		    }

		    /*
		     * Track position adjustment.  Move the position if we are NOT:
		     *	- Cued
		     *	- Waiting to record, but not recording.
		     */
		    if (!(track->options & MUSIC_TRACK_OPTION_CUED) &&
			(track_num != loop->record_track ||
			 (track->options & MUSIC_TRACK_OPTION_RECORDING)))
		    {
			/*
			 * Data comes in as left and right channels.  If this is the
			 * first channel, then advance the position to the next channel.
			 */
			if (!(i & 1))
			{
			    track->pos += 4;
			}
			else
			{
			    /*
			     * If MIDI synchronized then we don't allow speed change or
			     * reverse.
			     */
			    if ((track->options & MUSIC_TRACK_OPTION_MIDISYNC))
			    {
				/*
				 * Assume no time correction.  We will change this if
				 * we need to.
				 */
				pos_adjust = 4;
				
				/*
				 * MIDI speed correction is done by either skipping
				 * a sample or staying on the same sample twice.
				 */
				if (track->midi_pos_adjust_timer == 0 &&
				    track->midi_pos_adjust_num > 0)
				{
				    /*
				     * If desired adjustment is backward, then stay at this
				     * sample.
				     */
				    if (track->midi_pos_adjust < 0 && track->midi_sync_count)
				    {
					pos_adjust = -4;
					track->midi_sync_count -= (1 << MIDI_FRACTION_BITS);
					track->midi_pos_adjust_num--;
				    }
				    else if (track->midi_pos_adjust > 0)
				    {
					pos_adjust = 12;
					track->midi_sync_count += (1 << MIDI_FRACTION_BITS);
					track->midi_pos_adjust_num--;
				    }

				    track->midi_pos_adjust_timer = MUSIC_TRACK_MIDI_ADJUST_SPACING;
				}
				else if (track->midi_pos_adjust_timer > 0)
				    track->midi_pos_adjust_timer--;
				
				track->midi_sync_count += (1 << MIDI_FRACTION_BITS);
				track->pos += pos_adjust;
			    }
			    /*
			     * Not MIDI sync'ing.  Handle reverse and speed changes.
			     */
			    else
			    {
				/*
				 * Backwards or forwards?
				 */
				if (track->options & MUSIC_TRACK_OPTION_REVERSE)
				    pos_adjust = -12;
				else
				    pos_adjust = 4;

				/*
				 * Track may be running at a slower speed.  To do this
				 * we will stay on the current pair of samples for an
				 * additional cycle.
				 */
				track->slow_cycle += track->slow_cycle_inc;

				if (track->slow_cycle < 400)
				    pos_adjust = -4;
				else
				    track->slow_cycle -= 400;

				track->pos += pos_adjust;
			    }
			    
			    /*
			     * Calculate MIDI clock out if this is track 1.
			     */
			    if (track_num == 0 && 
				sport_info.midi_sync_out_enabled &&
				sport_info.beats_per_measure > 0 &&
				track->pos >= sport_info.next_clock_pos &&
				loop->record_track != 0 &&
				loop->bounce_track != 0 &&
				track->length > 0 &&
				(sport_info.next_clock_pos > 0 || 
				 track->pos < track->length / 2))
			    {
				short nsegments = 
				    sport_info.beats_per_measure * 24;
				    
				if (sport_info.clock_segment == 0)
				{
				    if (track->options & 
					MUSIC_TRACK_OPTION_PLAYING)
				    {
					sport_info.send_clock = 2;
				    }
				    else
				    {
					sport_info.send_clock = 3;
				    }
				}
				else
				{
				    sport_info.send_clock = 1;
				}

				sport_info.clock_segment++;
				if (sport_info.clock_segment >= nsegments)
				    sport_info.clock_segment = 0;
			    
				sport_info.next_clock_pos = 
				    ((track->length / nsegments) * 
				     sport_info.clock_segment);
			    }
			}
		    }
		}

#ifdef USE_SPORT_OUT_FILTER
		sport_out_filter[i & 1] >>= 1;
		sport_out_filter[i & 1] += (sample_out >> 1);
		sample_out = sport_out_filter[i & 1];
#endif

 		/*
		  Apply master volume.  This will allow us to temporarily
		  exceed 24 bits and then reign the values back in before 
		  sending to the DAC.
		*/
		if (sport_info.master_volume > 0)
		    sample_out <<= sport_info.master_volume;
		else if (sport_info.master_volume < 0)
		    sample_out >>= -sport_info.master_volume;

		/* 
		   Check for overflow.  If we overflow 24 bits, then we
		   need to limit to 24 bits or else we will get some very
		   harsh distortion.
		*/
		if (sample_out > sport_info.max_out_value)
		    sport_info.max_out_value = sample_out;

		if (sample_out > 0x007fffff)
		    sample_out = 0x007fffff;
		else if (sample_out < - 0x007fffff)
		    sample_out = - 0x007fffff;

		efx_bufp[i] = sample_out;

 		/*
		  Apply master volume.  This will allow us to temporarily
		  exceed 24 bits and then reign the values back in before 
		  sending to the DAC.
		*/
		if (sport_info.master_volume > 0)
		    aux_out <<= sport_info.master_volume;
		else if (sport_info.master_volume < 0)
		    aux_out >>= -sport_info.master_volume;

		if (sport_info.master_volume > 0)
		    aux2_out <<= sport_info.master_volume;
		else if (sport_info.master_volume < 0)
		    aux2_out >>= -sport_info.master_volume;

		/* 
		   Check for overflow.  If we overflow 24 bits, then we
		   need to limit to 24 bits or else we will get some very
		   harsh distortion.
		*/
		if (aux_out > sport_info.max_out_value)
		    sport_info.max_out_value = aux_out;

		if (aux_out > 0x007fffff)
		    aux_out = 0x007fffff;
		else if (aux_out < - 0x007fffff)
		    aux_out = - 0x007fffff;

		if (aux2_out > sport_info.max_out_value)
		    sport_info.max_out_value = aux2_out;

		if (aux2_out > 0x007fffff)
		    aux2_out = 0x007fffff;
		else if (aux2_out < - 0x007fffff)
		    aux2_out = - 0x007fffff;

		aux_bufp[i * 2] = aux_out;
		aux_bufp[i * 2 + 1] = aux2_out;
	    }

	    fx_bufn++;
	    if (fx_bufn >= SPORT_DMA_BUFS)
		fx_bufn = 0;
	}

	sport_info.next_fx_bufn = rx_bufn;
    }

    spin_unlock_irqrestore(&sport_dma_lock, flags);
    looper_time_stamps[looper_time_stamp_idx] = *pTCOUNT;
    looper_time_stamp_idx = (looper_time_stamp_idx + 1) & (NUM_LOOPER_TIME_STAMPS - 1);
    
#if 0
    *pFIO_FLAG_C = 0x0010;
#endif

    return IRQ_HANDLED;
}

void initialize_loop(struct sport_s *s, struct music_loop_s *loop)
{
    int i;
    int j;
    
    loop->bounce_track = -1;
    loop->record_track = -1;
    loop->overdub_track = -1;
    loop->master_track = -1;
    loop->replace_track = -1;
    
    loop->undo_track = -1;
    for (j = 0; j < (10485760 / LOOPER_BLOCK_SIZE) + 1; j++)
	loop->undo_audio[j] = (loop_audio_t *) looper_alloc();

    for ( ; j < LOOPER_MAX_BLOCKS; j++)
	loop->undo_audio[j] = NULL;

    for (i = 0; i < 8; i++)
    {
	loop->tracks[i].fadeout = 0;
	loop->tracks[i].fadein = 0;
	loop->tracks[i].fadein_level[0] = 0;
	loop->tracks[i].fadein_level[1] = 0;
	loop->tracks[i].fadeout_level[0] = 0;
	loop->tracks[i].fadeout_level[1] = 0;
	loop->tracks[i].options = 0;
	loop->tracks[i].length = 0;
	loop->tracks[i].original_length = 0;
	loop->tracks[i].virtual_start_offset = 0;
	loop->tracks[i].pos = 0;
	loop->tracks[i].levels[0] = 0;
	loop->tracks[i].levels[1] = 0;
	loop->tracks[i].target_level = 0;
	loop->tracks[i].pan = 0;
	loop->tracks[i].per_channel_targets[0] = 0;
	loop->tracks[i].per_channel_targets[1] = 0;
	loop->tracks[i].level_change_count = 0;
	loop->tracks[i].feedback = 128;
	loop->tracks[i].quantized_steps = 1;
	loop->tracks[i].midi_errors_idx = 0;

	for (j = 0; j < LOOPER_MAX_BLOCKS; j++)
	    loop->tracks[i].audio[j] = NULL;
    }

    s->loop = loop;
}

unsigned long get_sclk(void);
static struct proc_dir_entry *looper_proc_node = NULL;

static int read_looper_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    unsigned long flags;
    int len = 0;
    short i;
    long intr_time = 0;
    long not_intr_time = 0;
    long l;
    
    spin_lock_irqsave(&sport_dma_lock, flags);

    for (i = 0; i < NUM_LOOPER_TIME_STAMPS; i += 2)
    {
	l = looper_time_stamps[i] - looper_time_stamps[i+1];
	if (l < 0)
	    l += *pTPERIOD;
	intr_time += l;
	
	if (i > 0)
	{
	    l = looper_time_stamps[i-1] - looper_time_stamps[i];
	    if (l < 0)
		l += *pTPERIOD;
	    not_intr_time += l;
	}
    }

    spin_unlock_irqrestore(&sport_dma_lock, flags);

    len += sprintf(page + len, "LP1 Looper statistics:\n");
    len += sprintf(page + len, "        TPERIOD, TCOUNT: %ld, %ld\n", 
		   *pTPERIOD, *pTCOUNT);
    len += sprintf(page + len, "      Average intr time: %ld\n", 
		   intr_time / (NUM_LOOPER_TIME_STAMPS / 2));
    len += sprintf(page + len, "     Average other time: %ld\n", 
		   not_intr_time / (NUM_LOOPER_TIME_STAMPS / 2));
    len += sprintf(page + len, "        TPERIOD, TCOUNT: %ld, %ld\n\n", 
		   *pTPERIOD, *pTCOUNT);
    len += sprintf(page + len, "       MIDI avg counter: %ld\n", 
		   sport_avg_midi_counter() >> MIDI_FRACTION_BITS);

    len += sprintf(page + len, "Track 1: flags %08lx, fade lvl %d %d, fade dir %d %d\n",
		   sport_info.loop->tracks[0].options,
		   sport_info.loop->tracks[0].play_fade_level[0],
		   sport_info.loop->tracks[0].play_fade_level[1],
		   sport_info.loop->tracks[0].play_fade_direction[0],
		   sport_info.loop->tracks[0].play_fade_direction[1]);

    i = 0;
    while (len < count - 100 && i < 1000)
    {
	len += sprintf(page + len, "   %7ld%7ld%7ld%7ld%7ld%7ld%7ld%7ld%7ld%7ld\n",
		       looper_midi_clock_samples[i+0] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+1] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+2] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+3] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+4] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+5] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+6] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+7] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+8] >> MIDI_FRACTION_BITS,
		       looper_midi_clock_samples[i+9] >> MIDI_FRACTION_BITS);
	i += 10;
    }
	
    
    len += sprintf(page + len, "\n");

    return len;
}

int write_looper_proc(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int i;
    
    looper_midi_clock_sample_idx = 0;
    for (i = 0; i < 1000; i++)
	looper_midi_clock_samples[i] = 0;
    
    return count;
}
    
int init_module(void)
{
    volatile dmasg_t *sgp;
    short i;
    unsigned long sclk_freq = get_sclk();
    unsigned short n_sport_clk_div;
    unsigned short n_uart_timer_div;

    looper_time_stamps = (long *) 
	l1_data_A_sram_alloc(NUM_LOOPER_TIME_STAMPS * sizeof(*looper_time_stamps));
    
    looper_proc_node = create_proc_read_entry("looper", 0444, NULL, read_looper_proc, NULL);
    looper_proc_node->write_proc = write_looper_proc;

    n_sport_clk_div = (unsigned short) (((sclk_freq + 64) / 128 + 24000) 
					/ SPORT_SAMPLING_RATE);
    n_uart_timer_div = (unsigned short) ((sclk_freq + 19200) / 38400);
    
    DDEBUG(("%ld, %d, %d\n", sclk_freq, n_sport_clk_div, n_uart_timer_div));

    looper_mem_init();

    initialize_loop(&sport_info, &audio_loop);

#if 0
    /* Initialize analyzer pulse */
    *pFIO_DIR |= 0x0010;
    *pFIO_FLAG_C = 0x0010;
#endif
    

#ifdef SPORT_USE_L1_MEMORY
    sport_dma_buf = 
	(long *) l1_data_A_sram_alloc(SPORT_DMA_BYTES * SPORT_DMA_BUFS);
    sport1_dma_buf = 
	(long *) l1_data_A_sram_alloc(SPORT_DMA_BYTES * SPORT_DMA_BUFS * 2);
#else
    sport_dma_buf = looper_alloc();
    sport1_dma_buf = looper_alloc();
#endif
    DDEBUG(("DMA buffers: %x\n", (unsigned int) sport_dma_buf));
    
    sport_rx_sg = 
	(long *) l1_data_A_sram_alloc(SPORT_SG_BYTES * SPORT_DMA_BUFS);
    sport1_rx_sg = 
	(long *) l1_data_A_sram_alloc(SPORT_SG_BYTES * SPORT_DMA_BUFS);
    DDEBUG(("DMA descriptors: %x\n", (unsigned int) sport_rx_sg));
    
    sport_major = register_chrdev(SPORT_MAJOR_NR, "sport", &sport_fops);
    DDEBUG(("Registered SPORT device as %d\n", sport_major));

    /*
      Set up error handler for SPORT.  We never expect an error because SPORT
      will be serviced by DMA.
    */
    if (request_irq(IRQ_SPORT0_ERROR, &sport_error_handler, 
		    SA_SHIRQ, "SPORT Error", &sport_info))
    {
 	DDEBUG(("Failed to allocate SPORT error interrupt, %d\n", 
		IRQ_SPORT0_ERROR));
	return -ENODEV;
    }

    enable_irq(IRQ_SPORT0_ERROR);

    if (request_irq(IRQ_SPORT1_ERROR, &sport_error_handler, 
		    SA_SHIRQ, "SPORT Error", &sport_info))
    {
 	DDEBUG(("Failed to allocate SPORT error interrupt, %d\n", 
		IRQ_SPORT1_ERROR));
	return -ENODEV;
    }

    enable_irq(IRQ_SPORT1_ERROR);

    /*
      Set up DMA channels for SPORT.
     */
    if (request_dma(CH_SPORT0_RX, "SPORT RX") < 0)
    {
	DDEBUG(("Failed to allocate SPORT RX interrupt\n"));
	return -ENODEV;
    }

    if (request_dma(CH_SPORT0_TX, "SPORT TX") < 0)
    {
	DDEBUG(("Failed to allocate SPORT TX interrupt\n"));
	return -ENODEV;
    }

    if (request_dma(CH_SPORT1_TX, "SPORT TX") < 0)
    {
	DDEBUG(("Failed to allocate SPORT TX interrupt\n"));
	return -ENODEV;
    }

    if (set_dma_callback(CH_SPORT0_RX, &sport_dma_rx_handler, &sport_info))
    {
	DDEBUG(("Failed to set dma callback for SPORT RX\n"));
	return -ENODEV;
    }

    /*
      Initialize DMA descriptors for main in and out.
    */
    sgp = (dmasg_t *) sport_rx_sg;
    sgp->next_desc_addr = (unsigned long) sgp + SPORT_SG_BYTES;
    sgp->start_addr = (unsigned long) sport_dma_buf;
    DDEBUG(("DMA desc at %x, buffer at %lx, next %lx\n", 
	   (unsigned) sgp, sgp->start_addr, sgp->next_desc_addr));
    
    for (i = 1; i < SPORT_DMA_BUFS - 1; i++)
    {
	sgp = (dmasg_t *) sgp->next_desc_addr;
	sgp->next_desc_addr = (unsigned long) sgp + SPORT_SG_BYTES;
	sgp->start_addr = 
	    (unsigned long) sport_dma_buf + (SPORT_DMA_BYTES * i);

	DDEBUG(("DMA desc at %x, buffer at %lx, next %lx\n", 
	       (unsigned) sgp, sgp->start_addr, sgp->next_desc_addr));
    }
    
    sgp = (dmasg_t *) sgp->next_desc_addr;
    sgp->next_desc_addr = (unsigned long) sport_rx_sg;
    sgp->start_addr = (unsigned long) sport_dma_buf + SPORT_DMA_BYTES * i;
    DDEBUG(("DMA desc at %x, buffer at %lx, next %lx\n", 
	   (unsigned) sgp, sgp->start_addr, sgp->next_desc_addr));

    /*
      Initialize DMA descriptors for auxilliary outputs.
    */
    sgp = (dmasg_t *) sport1_rx_sg;
    sgp->next_desc_addr = (unsigned long) sgp + SPORT_SG_BYTES;
    sgp->start_addr = (unsigned long) sport1_dma_buf;
    DDEBUG(("DMA desc at %x, buffer at %lx, next %lx\n", 
	   (unsigned) sgp, sgp->start_addr, sgp->next_desc_addr));
    
    for (i = 1; i < SPORT_DMA_BUFS - 1; i++)
    {
	sgp = (dmasg_t *) sgp->next_desc_addr;
	sgp->next_desc_addr = (unsigned long) sgp + SPORT_SG_BYTES;
	sgp->start_addr = 
	    (unsigned long) sport1_dma_buf + (SPORT_DMA_BYTES * 2 * i);

	DDEBUG(("DMA desc at %x, buffer at %lx, next %lx\n", 
	       (unsigned) sgp, sgp->start_addr, sgp->next_desc_addr));
    }
    
    sgp = (dmasg_t *) sgp->next_desc_addr;
    sgp->next_desc_addr = (unsigned long) sport1_rx_sg;
    sgp->start_addr = (unsigned long) sport1_dma_buf + SPORT_DMA_BYTES * 2 * i;
    DDEBUG(("DMA desc at %x, buffer at %lx, next %lx\n", 
	   (unsigned) sgp, sgp->start_addr, sgp->next_desc_addr));

    /*
      Initialize DMA registers for main input and output.
     */
    set_dma_start_addr(CH_SPORT0_RX, (unsigned int) sport_dma_buf);
    set_dma_x_count(CH_SPORT0_RX, SPORT_DMA_LONGS);
    set_dma_x_modify(CH_SPORT0_RX, 4);

    set_dma_start_addr(CH_SPORT0_TX, 
		       (unsigned int) sport_dma_buf + SPORT_DMA_BYTES);
    set_dma_x_count(CH_SPORT0_TX, SPORT_DMA_LONGS);
    set_dma_x_modify(CH_SPORT0_TX, 4);

    /*
      Initialize DMA registers for auxilliary outputs.
     */
    set_dma_start_addr(CH_SPORT1_TX, 
		       (unsigned int) sport1_dma_buf + SPORT_DMA_BYTES * 2);
    set_dma_x_count(CH_SPORT1_TX, SPORT_DMA_LONGS * 2);
    set_dma_x_modify(CH_SPORT1_TX, 4);

    /* Start RX on buffer 0, TX on buffer 1, software processing on buffer 2 */
    *pDMA1_NEXT_DESC_PTR = sport_rx_sg;
    *pDMA2_NEXT_DESC_PTR = sport_rx_sg + SPORT_SG_LONGS;

    /* Aux 1 output */
    *pDMA4_NEXT_DESC_PTR = sport1_rx_sg + SPORT_SG_LONGS;

    SSYNC;
    DDEBUG(("SG pointers set to %x, %x, %x\n", 
	   (unsigned) *pDMA1_NEXT_DESC_PTR, (unsigned) *pDMA2_NEXT_DESC_PTR, 
	   (unsigned) *pDMA4_NEXT_DESC_PTR));

    set_dma_config(CH_SPORT0_RX, 0x748b);
    set_dma_config(CH_SPORT0_TX, 0x7409);
    set_dma_config(CH_SPORT1_TX, 0x7409);

    /* Configure SPORT and start data flowing */
    *pSPORT0_TCLKDIV = n_sport_clk_div - 1;
    *pSPORT0_RCLKDIV = n_sport_clk_div - 1;
    *pSPORT1_TCLKDIV = n_sport_clk_div - 1;

    *pSPORT0_TFSDIV = 31;
    *pSPORT0_RFSDIV = 31;
    *pSPORT1_TFSDIV = 31;

    *pTIMER1_CONFIG = 0x0009;	/* A/D master clock */
    *pTIMER1_PERIOD = n_sport_clk_div / 2;
    *pTIMER1_WIDTH = n_sport_clk_div / 4;
    *pTIMER_ENABLE = 0x0002;

    *pTIMER2_CONFIG = 0x0009;	/* Display communications clock */
    *pTIMER2_PERIOD = n_uart_timer_div;
    *pTIMER2_WIDTH = n_uart_timer_div / 2;
    *pTIMER_ENABLE = 0x0006;

    *pSPORT0_RCR2 = 0x0217;
#ifdef SPORT_MULTIPLE_CLOCKS
    *pSPORT0_RCR1 = 0x4607;	/* internal clock */
#else
    *pSPORT0_RCR1 = 0x4405;	/* external clock */
#endif

    *pSPORT0_TCR2 = 0x0217;
    *pSPORT0_TCR1 = 0x4607;	/* internal clock */

    *pSPORT1_TCR2 = 0x0317;	/* primary and secondary enabled */
    *pSPORT1_TCR1 = 0x4607;	/* internal clock */
    SSYNC;

    return 0;
}


void cleanup_module(void)
{
    /* Stop SPORTS */
    *pSPORT0_TCR1 = 0;
    *pSPORT0_RCR1 = 0;
    SSYNC;

    /* Disable DMA */
    set_dma_config(CH_SPORT0_RX, 0);
    set_dma_config(CH_SPORT0_TX, 0);

    /* Disable PWM */
    *pTIMER_DISABLE = 0x0002;
    SSYNC;

    /* Free DMA channels */
    free_dma(CH_SPORT0_RX);
    free_dma(CH_SPORT0_TX);

    /* Disable and free error IRQ */
    disable_irq(IRQ_SPORT0_ERROR);
    free_irq(IRQ_SPORT0_ERROR, &sport_info);
    
    unregister_chrdev(sport_major, "sport");
}

void sport_dump_addresses()
{
    DDEBUG(("cleanup_module: %lx\n", (unsigned long) cleanup_module));
    DDEBUG(("init_module: %lx\n", (unsigned long) init_module));
    DDEBUG(("initialize_loop: %lx\n", (unsigned long) initialize_loop));
    DDEBUG(("sport_dma_rx_handler: %lx\n", (unsigned long) sport_dma_rx_handler));
    DDEBUG(("noise_gate: %lx\n", (unsigned long) noise_gate));
    DDEBUG(("sport_check_clock_out: %lx\n", (unsigned long) sport_check_clock_out));
}

MODULE_LICENSE("Looperlative Audio Products Proprietary");
MODULE_AUTHOR("Robert Amstadt <bob@looperlative.com>");
MODULE_DESCRIPTION("Looperlative LP1 looping controller");
