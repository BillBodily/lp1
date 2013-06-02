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

#ifndef __LOOPS_H__
#define __LOOPS_H__

#include <linux/types.h>

/*
 * AUDIO_SIZE can be 16 or 32.
 *
 * Now for the strange stuff.  The code was originally only designed to handle 32-bit audio.
 * This means that ->len and ->pos are all based on 4-bytes per sample.  For speed reasons,
 * I don't want to skip 2 bytes between each audio sample.  So, I need to pack the audio
 * samples.  This means that position within a block needs to be scaled for the audio size.
 *
 * In either case, the number of samples per block is (LOOPER_BLOCK_SIZE / 4).  If we are
 * recording 16-bit audio, then half the block will be empty.  This seems silly, but it is the
 * easiest way to convert the current software.  Future revisions will likely remove this
 * wasting of memory.
 */
#define AUDIO_SIZE	16

#if (AUDIO_SIZE == 16)
typedef int16_t loop_audio_t;

#define AUDIO_OFFSET_SHIFT	1
#define AUDIO_DATA_SHIFT	10
#else
typedef int32_t loop_audio_t;

#define AUDIO_OFFSET_SHIFT	0
#define AUDIO_DATA_SHIFT	0
#endif

#include "loop_mem.h"

#define MUSIC_TRACK_OPTION_PLAYING	0x00000001
#define MUSIC_TRACK_OPTION_RECORDING	0x00000002
#define MUSIC_TRACK_OPTION_BOUNCEFROM	0x00000004
#define MUSIC_TRACK_OPTION_ASYNC	0x00000010
#define MUSIC_TRACK_OPTION_MIDISYNC	0x00000020
#define MUSIC_TRACK_OPTION_SYNC		0x00000040
#define MUSIC_TRACK_OPTION_REVERSEFADE	0x00000080

#define MUSIC_TRACK_OPTION_REVERSE	0x00000100
#define MUSIC_TRACK_OPTION_REPLACEPLUS	0x00000200
#define MUSIC_TRACK_OPTION_SCRAMBLED	0x00000400
#define MUSIC_TRACK_OPTION_SCR_PENDING	0x00000800
#define MUSIC_TRACK_OPTION_SCR_ENDING	0x00001000
#define MUSIC_TRACK_OPTION_HALFSPEED	0x00002000

#define MUSIC_TRACK_OPTION_DELAYEDPLAY	0x00010000
#define MUSIC_TRACK_OPTION_DELAYEDSTOP	0x00020000
#define MUSIC_TRACK_OPTION_CUED		0x00040000
#define MUSIC_TRACK_OPTION_BOUNCEFADE	0x00080000
#define MUSIC_TRACK_OPTION_QUANTIZED	0x00100000
#define MUSIC_TRACK_OPTION_RETRIGGER	0x00200000

#define MUSIC_BOUNCE_OPTION_MIXINPUT	0x0001
#define MUSIC_BOUNCE_OPTION_ENDRECORD	0x0002

#define MUSIC_TRACK_MIDI_ADJUST_SPACING	22

#define MIDI_FRACTION_BITS		10

struct music_track_s
{
    unsigned long options;
    unsigned int length;
    unsigned int original_length;
    volatile int pos;
    int virtual_start_offset;
    short fadeout;
    short fadein;
    short levels[2];
    short per_channel_targets[2];
    short target_level;
    short level_change_count;
    short pan;
    
    short feedback;
    short fadein_level[2];
    short fadeout_level[2];

    long fadein_samples[2];
    long fadeout_samples[2];

    short slow_cycle;
    short slow_cycle_inc;
    loop_audio_t *audio[LOOPER_MAX_BLOCKS];
    short scrambled[LOOPER_MAX_BLOCKS];

    /* MIDI sync variables */
    long quarternote_period;
    long midi_sync_count;	// >= 0, <= midiclock_period
    long midiclock_period;
    long midiclock_offset;	// midi_sync_count at loop start
    long midi_error;
    long midi_errors[16];
    long midi_errors_idx;
    unsigned int length_check_point; // Loop length recheck point based on MIDI error

    /*
     * Our goal is to hide MIDI adjustments.  We do this by infrequently
     * skipping or repeating single samples.
     *
     * midi_pos_adjust is the adjustment (either +1, -1 or 0)
     * midi_pos_adjust_timer is the number of samples until next adjustment
     * midi_pos_adjust_num is the number of adjustments left to do.
     */
    int midi_pos_adjust;
    unsigned short midi_pos_adjust_timer;
    unsigned short midi_pos_adjust_num;
    
    /* Start/stop fading */
    short play_fade_direction[2];
    short play_fade_level[2];

    /* Replace blending */
    short replace_blend_direction;
    short replace_blend_level;

    /* Quantized functions */
    short quantized_steps;
};

struct music_loop_s
{
    short record_track;
    short overdub_track;
    short replace_track;
    short master_track;
    short bounce_track;
    unsigned short bounce_options;
    int bounce_end_pos;
    struct music_track_s tracks[8];

    short undo_track;
    loop_audio_t *undo_audio[LOOPER_MAX_BLOCKS];
};

#endif /* __LOOPS_H__ */
