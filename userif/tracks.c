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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include "../modules/sport.h"
#include "../modules/spi.h"
#include "userif.h"
#include "menus.h"
#include "tracks.h"
#include "config.h"
#include "midisync.h"

extern int spifd;
extern int lp1sd_mount_result;

void midi_report_period(unsigned long midi_period);

#define TRACK_DISPLAY_VOLUME	0
#define TRACK_DISPLAY_FEEDBACK	1
#define TRACK_DISPLAY_BPM	2
#define TRACK_DISPLAY_NONE	3
#define TRACK_DISPLAY_LASTCMD	4
#define TRACK_DISPLAY_POSITION	5
#define TRACK_DISPLAY_SPEED	6
#define TRACK_DISPLAY_PAN	7

#define TRACK_DISPLAY_TIMEOUT	25

unsigned char track_aux1_mask = 0;
unsigned char track_aux2_mask = 0;

int track_everything_except = 0;
int track_current_track = 1;
int track_current_loop = 1;
unsigned int track_current_status = 0;
short track_current_master;
short track_level_display = TRACK_DISPLAY_VOLUME;
char track_last_cmd[10];
short track_display_timeout = TRACK_DISPLAY_TIMEOUT;
unsigned short track_last_playing_mask = 0;
struct sport_loop_status_s last_ls;

static unsigned char audio_buffer[64 * 1024];

/*
  Swell variables
*/
short saved_track_levels[8] = {0,0,0,0,0,0,0,0};
short target_track_levels[8] = {0,0,0,0,0,0,0,0};
short track_level_deltas[8] = {0,0,0,0,0,0,0,0};
short track_level_deltacountmax[8] = {0,0,0,0,0,0,0,0};
short track_level_deltacount[8] = {0,0,0,0,0,0,0,0};

#define SWELL_TIME	10000	/* in milliseconds */

/*
  Bounce variables.
*/
short track_bounce_track = 0;
unsigned short track_bounce_from_mask = 0;

char track_status_string[32] = "";

char *track_get_title(void);

struct menu_item_s track_items[] = 
{
    { track_status_string, NULL, NULL, 1 },
    { "Save Audio", track_key_savetrackaudio, NULL, 0 },
    { "Load Audio", track_key_loadtrackaudio, NULL, 0 },
    { "MIDI Sync Rec", track_key_recdub, NULL, 1 },
    { "Sync Rec/Dub", track_key_recdub, NULL, 2 },
    { "Stop Now", track_key_stop, NULL, 1 },
    { "Play Now", track_key_play, NULL, 1 },
    { "Erase", track_key_erase, NULL, 0 },
    { "Level", track_key_level, NULL, LP_NO_ARG },
    { "Feedback", track_key_feedback, NULL, LP_NO_ARG },
    { "Multiply", track_key_multiply, NULL, 0 },
    { "Set As Master", track_key_setmaster, NULL, 0 },
    { "Next Track", track_key_changetrack, NULL, 1 },
    { "Prev Track", track_key_changetrack, NULL, -1 },
    { "Reverse", track_key_reverse, NULL, 0 },
    { "Half Speed", track_key_halfspeed, NULL, 0 },
    { "Play Retrigger", track_key_play, NULL, 2 },
    { "Cue", track_key_cue, NULL, 0 },
    { "Replace", track_key_replace, NULL, 0 },
    { "Replace+Original", track_key_replace, NULL, 1 },
    { "Q Replace", track_key_qreplace, NULL, 0 },
    { "Q Replace+Orig", track_key_qreplace, NULL, 1 },
    { "Set Q Steps", track_key_quantization_steps, NULL, 0 },
    { "Assign To Aux 1", track_key_aux, NULL, 1 },
    { "Assign To Aux 2", track_key_aux, NULL, 2 },
    { "Assign To Main", track_key_aux, NULL, 0 },
    { "Fast Scramble", track_key_scramble, NULL, 1 },
    { "Medium Scramble", track_key_scramble, NULL, 4 },
    { "Slow Scramble", track_key_scramble, NULL, 16 },
    { "Replay/All Stop", track_key_replaystopall, NULL, 0 },
    { "Copy", track_key_copy, NULL, 0 },
    { "Bounce", track_key_bounce, NULL, SPORT_SYNC_NONE },
    { "Sync Bounce", track_key_bounce, NULL, SPORT_SYNC_INTERNAL },
    { "MIDI Sync Bounce", track_key_bounce, NULL, SPORT_SYNC_MIDI },
    { "Pan", track_key_pan, NULL, LP_NO_ARG },
    { "Fade/Swell", track_key_fadeswell, NULL, 0 },
    { "Undo Overdub", track_key_undo, NULL, 0 },
    { "Except Track", track_key_everythingexcept, NULL, 0 },
};

struct menu_s track_menu =
{
    NULL,
    track_get_title,
    &main_menu,
    track_items,
    sizeof(track_items) / sizeof(struct menu_item_s),
    NULL
};

struct menu_item_s all_track_items[] = 
{
    { "Stop Now", track_key_allstop, NULL, 1 },
    { "Fade/Swell", track_key_fadeswell, NULL, 0 },
    { "Play Now", track_key_allplay, NULL, 1 },
    { "Stop", track_key_allstop, NULL, 0 },
    { "Play", track_key_allplay, NULL, 0 },
    { "Erase", track_key_erase, NULL, 0 },
    { "Fade Time", config_set_volumefadetime, NULL, 0 },
    { "Master Level", track_key_master_level, NULL, 0 },
    { "Beats / Loop", track_key_beats_per_measure, NULL, 0 },
    { "Assign To Aux 1", track_key_aux, NULL, 1 },
    { "Assign To Aux 2", track_key_aux, NULL, 2 },
    { "Assign To Main", track_key_aux, NULL, 0 },
    { "Save Preset 0", track_key_savelevels, NULL, 0 },
    { "Save Preset 1", track_key_savelevels, NULL, 1 },
    { "Save Preset 2", track_key_savelevels, NULL, 2 },
    { "Save Preset 3", track_key_savelevels, NULL, 3 },
    { "Save Preset 4", track_key_savelevels, NULL, 4 },
    { "Save Preset 5", track_key_savelevels, NULL, 5 },
    { "Save Preset 6", track_key_savelevels, NULL, 6 },
    { "Save Preset 7", track_key_savelevels, NULL, 7 },
    { "Save Preset 8", track_key_savelevels, NULL, 8 },
    { "Save Preset 9", track_key_savelevels, NULL, 9 },
    { "Save Preset 10", track_key_savelevels, NULL, 10 },
    { "Use Preset 0", track_key_restorelevels, NULL, 0 },
    { "Use Preset 1", track_key_restorelevels, NULL, 1 },
    { "Use Preset 2", track_key_restorelevels, NULL, 2 },
    { "Use Preset 3", track_key_restorelevels, NULL, 3 },
    { "Use Preset 4", track_key_restorelevels, NULL, 4 },
    { "Use Preset 5", track_key_restorelevels, NULL, 5 },
    { "Use Preset 6", track_key_restorelevels, NULL, 6 },
    { "Use Preset 7", track_key_restorelevels, NULL, 7 },
    { "Use Preset 8", track_key_restorelevels, NULL, 8 },
    { "Use Preset 9", track_key_restorelevels, NULL, 9 },
    { "Use Preset 10", track_key_restorelevels, NULL, 10 },
};

struct menu_s all_track_menu =
{
    NULL,
    track_get_title,
    &main_menu,
    all_track_items,
    sizeof(all_track_items) / sizeof(struct menu_item_s),
    NULL
};

struct menu_item_s save_audio_failed_items[] = 
{
    { "installed", NULL, &track_menu, 0 },
};

struct menu_s save_audio_failed_menu =
{
    "No storage",
    NULL,
    &track_menu,
    save_audio_failed_items,
    sizeof(save_audio_failed_items) / sizeof(struct menu_item_s),
    NULL
};

char save_audio_success_msg[20] = "Audio saved";

struct menu_item_s save_audio_success_items[] = 
{
    { "", NULL, &track_menu, 0 },
};

struct menu_s save_audio_success_menu =
{
    save_audio_success_msg,
    NULL,
    &track_menu,
    save_audio_success_items,
    sizeof(save_audio_success_items) / sizeof(struct menu_item_s),
    NULL
};

struct menu_item_s load_audio_failed_items[] = 
{
    { "installed", NULL, &track_menu, 0 },
};

struct menu_s load_audio_failed_menu =
{
    "No storage",
    NULL,
    &track_menu,
    load_audio_failed_items,
    sizeof(load_audio_failed_items) / sizeof(struct menu_item_s),
    NULL
};

struct menu_item_s load_audio_success_items[] = 
{
    { "", NULL, &track_menu, 0 },
};

struct menu_s load_audio_success_menu =
{
    "Audio saved",
    NULL,
    &track_menu,
    load_audio_success_items,
    sizeof(load_audio_success_items) / sizeof(struct menu_item_s),
    NULL
};

struct menu_item_s song_name_items[100];
char song_names[100][20];
char song_name_msg[20] = "Save to";
struct menu_s song_name_menu =
{
    song_name_msg,
    NULL,
    &track_menu,
    song_name_items,
    0,
    NULL
};

char *track_get_title()
{
    static char title[40];
    static char track_name[10];
    struct sport_loop_status_s ls;
    unsigned short bpm = 0;
    int display_track = 1;
    unsigned char mask;
    int speed = 400;

    bpm = midi_get_bpm();

    if (track_current_track > 0 && track_everything_except)
    {
	sprintf(track_name, "Except %d", track_current_track);
	display_track = track_current_track;
    }
    else if (track_current_track > 0)
    {
	sprintf(track_name, "%cTrack %d", 
		(track_current_track == track_current_master) ? '*' : ' ',
		track_current_track);
	display_track = track_current_track;
    }
    else if (track_current_track == 0)
	strcpy(track_name, "All Trks");
    else if (track_current_track < 0)
    {
	sprintf(track_name, " Group %d", -track_current_track);
	mask = looper_config.track_groups[-1 - track_current_track];
	
	for (display_track = 1; display_track < 8; display_track++)
	    if (mask & (1 << (display_track - 1)))
		break;
    }

    if (display_track >= 1 && display_track <= 8)
    {
	ioctl(audiofd, CMD_SPORT_LOOPSTATUS, &ls);
	speed = ls.track_speeds[display_track - 1];
	
	if (speed == 400)
	    sprintf(track_status_string, "Sp 1.00 ");
	else
	    sprintf(track_status_string, "Sp 0.%02.2d ", (speed / 4) % 100);
    }

    if (track_level_display == TRACK_DISPLAY_BPM)
    {
	sprintf(title, "%s BPM%d", track_name, bpm);
    }
    else if (track_level_display == TRACK_DISPLAY_FEEDBACK)
    {
	sprintf(title, "%s Fbk%+d", track_name,
		looper_config.track_feedbacks[display_track - 1]);
    }
    else if (track_level_display == TRACK_DISPLAY_PAN)
    {
	sprintf(title, "%s Pan%+d", track_name,
		looper_config.track_pans[display_track - 1]);
    }
    else if (track_level_display == TRACK_DISPLAY_VOLUME)
    {
	sprintf(title, "%s Lvl%+d", track_name,
		looper_config.track_gains[display_track - 1]);
    }
    else if (track_level_display == TRACK_DISPLAY_SPEED)
    {
	if (speed == 400)
	    sprintf(title, "%s Sp 1.00", track_name);
	else
	    sprintf(title, "%s Sp 0.%02.2d", track_name, (speed / 4) % 100);
    }
    else if (track_level_display == TRACK_DISPLAY_LASTCMD)
    {
	sprintf(title, "%s : %s\x1f\x72\x01%s", 
		track_name, pad_string( track_last_cmd, 5 ), track_last_cmd);
    }
    else
    {
	sprintf(title, "%s", track_name);
    }
    
    return title;
}

void track_key_savesong(struct menu_s *parent, int displayfd, int arg)
{
    char filename[32];
    int afd;
    int trackfd;
    int nread;
    int total;
    int total_written;
    
    if (track_current_track < 1 || track_current_track > 8)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
	return;
    }

    sprintf(filename, "/mnt/song%d_%d.raw", arg+1, track_current_track);
    afd = open(filename, O_WRONLY | O_TRUNC | O_CREAT | O_SYNC, 0777);
    if (afd < 0)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
	return;
    }

    sprintf(filename, "/dev/sport%d", track_current_track);
    trackfd = open(filename, O_RDWR);
    if (trackfd < 0)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
	close(afd);
	return;
    }

    total = last_ls.track_length[track_current_track-1];
    total_written = 0;
    
    do
    {
	write( displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );

	sprintf(audio_buffer, "Song %d/%d %d%%", arg+1, track_current_track,
		total_written * 100 / total);
	write( displayfd, audio_buffer, strlen(audio_buffer) );

	nread = read(trackfd, audio_buffer, sizeof(audio_buffer));
	if (nread > 0)
	    write(afd, audio_buffer, nread);

	total_written += nread;
    }
    while (nread > 0);

    close(trackfd);
    close(afd);

    strcpy(save_audio_success_msg, "Audio saved");
    display_and_menu_set(&save_audio_success_menu, 0, 
			 menu_run_display, NULL,
			 g_current_menu, g_cur_menu_item, 
			 g_display_func, g_display_func_arg);
}

void track_key_savetrackaudio(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    
    if (lp1sd_mount_result < 0)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
    }
    else
    {
	for (i = 0; i < 50; i++)
	{
	    sprintf(song_names[i], "Song %d", i + 1);
	    song_name_items[i].displaystr = song_names[i];
	    song_name_items[i].enterfunc = track_key_savesong;
	    song_name_items[i].entermenu = NULL;
	    song_name_items[i].arg = i;
	}

	song_name_menu.n_items = i;

	strcpy(song_name_msg, "Save to");
	display_and_menu_set(&song_name_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
    }
}

void track_key_loadtrack(struct menu_s *parent, int displayfd, int arg)
{
    struct stat sbuf;
    char filename[32];
    int afd;
    int trackfd;
    int nread;
    int total;
    int total_written;
    int song_n;
    int track_n;
    
    if (track_current_track < 1 || track_current_track > 8)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
	return;
    }

    ioctl(audiofd, CMD_SPORT_STOPNOW, (unsigned long) track_current_track);
    ioctl(audiofd, CMD_SPORT_ERASE, (unsigned long) track_current_track);

    sscanf(song_names[arg], "Song %d Tr %d", &song_n, &track_n);
    sprintf(filename, "/mnt/song%d_%d.raw", song_n, track_n);
    afd = open(filename, O_RDONLY);
    if (afd < 0)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
	return;
    }

    sprintf(filename, "/dev/sport%d", track_current_track);
    trackfd = open(filename, O_RDWR);
    if (trackfd < 0)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
	close(afd);
	return;
    }

    fstat(afd, &sbuf);
    total = sbuf.st_size;
    total_written = 0;
    
    do
    {
	write(displayfd, "\x1f\x24\x00\x00\x00\x00", 6);

	sprintf(audio_buffer, "Song %d/%d %d%%", arg+1, track_current_track,
		total_written * 100 / total);
	write(displayfd, audio_buffer, strlen(audio_buffer));

	nread = read(afd, audio_buffer, sizeof(audio_buffer));
	if (nread > 0)
	    write(trackfd, audio_buffer, nread);

	total_written += nread;
    }
    while (nread > 0);

    strcpy(save_audio_success_msg, "Audio loaded");
    display_and_menu_set(&save_audio_success_menu, 0, 
			 menu_run_display, NULL,
			 g_current_menu, g_cur_menu_item, 
			 g_display_func, g_display_func_arg);
}

int track_loadtrackaudio_filter(const struct dirent *d)
{
    int song_n, track_n;
    char c;

    if (sscanf(d->d_name, "song%d_%d.raw", &song_n, &track_n, &c) == 2)
	return 1;
    else
	return 0;
}

void track_key_loadtrackaudio(struct menu_s *parent, int displayfd, int arg)
{
    struct dirent **namelist = NULL;
    int nentries;
    int song_n;
    int track_n;
    int i;
    
    if (lp1sd_mount_result < 0)
    {
	display_and_menu_set(&save_audio_failed_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
    }
    else
    {
	nentries = scandir("/mnt", &namelist, track_loadtrackaudio_filter,
			   alphasort);
	if (nentries > 50)
	    nentries = 50;
	
	for (i = 0; i < nentries; i++)
	{
	    sscanf(namelist[i]->d_name, "song%d_%d.raw", &song_n, &track_n);
	    sprintf(song_names[i], "Song %d Tr %d", song_n, track_n);
	    song_name_items[i].displaystr = song_names[i];
	    song_name_items[i].enterfunc = track_key_loadtrack;
	    song_name_items[i].entermenu = NULL;
	    song_name_items[i].arg = i;
	}

	song_name_menu.n_items = i;

	strcpy(song_name_msg, "Load from");
	display_and_menu_set(&song_name_menu, 0, 
			     menu_run_display, NULL,
			     g_current_menu, g_cur_menu_item, 
			     g_display_func, g_display_func_arg);
    }
}

void track_key_everythingexcept(struct menu_s *parent, int displayfd, int arg)
{
    track_everything_except ^= 1;

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "EXCP");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_setmaster(struct menu_s *parent, int displayfd, int arg)
{
    if (track_current_track > 0)
    {
	ioctl(audiofd, CMD_SPORT_SETMASTER, 
	      (unsigned long) track_current_track);
	track_current_master = track_current_track;
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "MSTR");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_reverse(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    unsigned char track_mask = track_get_track_mask();

    for (i = 1; i <= 8; i++)
    {
	if (track_mask & (1 << (i - 1)))
	    ioctl(audiofd, CMD_SPORT_REVERSE, (unsigned long) i);
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "REV");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_halfspeed(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    unsigned char track_mask = track_get_track_mask();

    for( i = 1; i <= 8; i++ )
    {
	if (track_mask & (1 << (i - 1)))
	{
	    ioctl(audiofd, CMD_SPORT_HALFSPEED, i);
	}
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy( track_last_cmd, "SPEED" );
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_fadeswell(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    int steps;
    int time_per_step;
    int swell_time;
    unsigned char track_mask = track_get_track_mask();
    short vmin = looper_config.volume_min;

    swell_time = looper_config.volume_fade_time * 1000;
    if (swell_time <= 0)
	swell_time = 1000;

    for( i = 0; i < 8; i++ )
    {
	if (track_mask & (1 << i))
	{
	    // Reverse fade/swell if in progress
	    if (arg == 0 && track_level_deltas[i] != 0)
	    {
		if (track_level_deltas[i] == 1)
		{
		    target_track_levels[i] = vmin;
		    track_level_deltas[i] = -1;
		}
		else
		{
		    target_track_levels[i] = saved_track_levels[i];
		    track_level_deltas[i] = 1;
		}
	    }
	    // SWELL if volume low or if SWELL command
	    else if ((looper_config.track_gains[i] < vmin - (vmin / 10) && 
		 arg == 0) || 
		arg == 2)
	    {
		if (saved_track_levels[i] < looper_config.track_gains[i])
		    continue;

		steps = saved_track_levels[i] - looper_config.track_gains[i];
		time_per_step = swell_time / steps;
		if (time_per_step < 20)
		    time_per_step = 20;

		track_level_deltacountmax[i] = time_per_step / 20;
		track_level_deltacount[i] = 0;
		target_track_levels[i] = saved_track_levels[i];
		track_level_deltas[i] = 1;
	    }
	    // Otherwise FADE
	    else
	    {
		if (looper_config.track_gains[i] > vmin - (vmin / 10))
		    saved_track_levels[i] = looper_config.track_gains[i];
		else
		    saved_track_levels[i] = 0;

		steps = looper_config.track_gains[i] - vmin;
		time_per_step = swell_time / steps;
		if (time_per_step < 20)
		    time_per_step = 20;

		track_level_deltacountmax[i] = time_per_step / 20;
		track_level_deltacount[i] = 0;
		target_track_levels[i] = vmin;
		track_level_deltas[i] = -1;
	    }
	}
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    switch (arg)
    {
      case 0: strcpy( track_last_cmd, "F/S" ); break;
      case 1: strcpy( track_last_cmd, "FADE" ); break;
      case 2: strcpy( track_last_cmd, "SWELL" ); break;
    }
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_recdub(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    int cmd;
    int t;
    int track = track_current_track;
    
    if (arg == 1)
    {
	cmd = CMD_MIDISYNC_RECORD;
	track_level_display = TRACK_DISPLAY_BPM;
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
    else if (arg == 2)
    {
	cmd = CMD_SYNC_RECORD;
	track_level_display = TRACK_DISPLAY_POSITION;
	strcpy(track_last_cmd, "SYN R");
    }
    else
    {
	cmd = CMD_SPORT_RECORD;
	track_level_display = TRACK_DISPLAY_POSITION;
	strcpy(track_last_cmd, "REC/D");
    }

    /* 
       If any track is in record or overdub state, then this button works 
       on that track instead of current track.
    */
    for (t = 1; t <= 8; t++)
    {
	trackstate = track_current_status >> ((t - 1) * 4);
	trackstate &= 0xf;

	if (trackstate == SPORT_TRACK_STATUS_RECORDING ||
	    trackstate == SPORT_TRACK_STATUS_OVERDUBBING)
	{
	    track = t;
	    break;
	}
    }

    if (track <= 0 && track_current_status == 0L)
    {
	track_current_track = 1;
	track_everything_except = 0;
	track = 1;
	
	display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
			      g_current_menu, g_cur_menu_item, g_display_func, 
			      g_display_func_arg );	
    }

    if (track > 0)
    {
	trackstate = track_current_status >> ((track - 1) * 4);
	trackstate &= 0xf;

	switch (trackstate)
	{
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	  case SPORT_TRACK_STATUS_REPLACING:
	    ioctl(audiofd, CMD_SPORT_PLAY, (unsigned long) track);
	    break;
	  case SPORT_TRACK_STATUS_RECORDING:
	  case SPORT_TRACK_STATUS_PLAYING:
	    ioctl(spifd, CMD_SPI_LED_GREENFLASH, (unsigned long) track);
	    ioctl(audiofd, CMD_SPORT_OVERDUB, (unsigned long) track);
	    break;
	  case SPORT_TRACK_STATUS_EMPTY:
	  case SPORT_TRACK_STATUS_STOPPED:
	    ioctl(audiofd, cmd, (unsigned long) track);
	    break;
	}
    }
}

void track_key_undo(struct menu_s *parent, int displayfd, int arg)
{
    if (ioctl(audiofd, CMD_SPORT_UNDO, 0) < 0)
    {
	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "NO UN");
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
    else
    {
	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "UNDO");
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
}

void track_key_copy(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long copyarg;
    unsigned long trackstate;
    short dest_track;

    if (track_current_track < 1 || track_current_track > 8)
	return;

    /*
      Find an empty track to copy to
    */
    for (dest_track = 8; dest_track >= 1; dest_track--)
    {
	trackstate = track_current_status >> ((dest_track - 1) * 4);
	trackstate &= 0xf;

	if (trackstate == SPORT_TRACK_STATUS_EMPTY)
	    break;
    }

    if (dest_track == 0)
	return;

    /*
      Start copy.
     */
    copyarg = ((unsigned long) dest_track << 16) | track_current_track;
    
    if (ioctl(audiofd, CMD_SPORT_COPY, copyarg) == 0)
    {
	track_current_track = dest_track;
	track_everything_except = 0;

	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "COPY");
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
}

void track_key_bounce(struct menu_s *parent, int displayfd, int arg)
{
    unsigned char track_mask = track_get_track_mask();
    struct sport_bounce_s bouncearg;
    unsigned long trackstate;
    short dest_track;

    track_level_display = TRACK_DISPLAY_POSITION;
    strcpy(track_last_cmd, "BOUNC");

    /*
      If we are already in bounce mode, then time to close out the track.
    */
    if (track_bounce_track > 0)
    {
	bouncearg.source_mask = 0;
	bouncearg.destination_track = track_bounce_track;
	bouncearg.sync_type = 0;

	if (ioctl(audiofd, CMD_SPORT_BOUNCE, (unsigned long) &bouncearg) == 0)
	{
	    track_bounce_track = 0;
	    track_bounce_from_mask = 0;
	}

	return;
    }

    /*
      Find an empty track to bounce to
    */
    for (dest_track = 8; dest_track >= 1; dest_track--)
    {
	trackstate = track_current_status >> ((dest_track - 1) * 4);
	trackstate &= 0xf;

	if (trackstate == SPORT_TRACK_STATUS_EMPTY)
	    break;
    }

    if (dest_track == 0)
	return;

    /*
      Start bounce.
     */
    bouncearg.source_mask = track_mask;
    bouncearg.destination_track = dest_track;
    bouncearg.sync_type = arg;

    if (ioctl(audiofd, CMD_SPORT_BOUNCE, (unsigned long) &bouncearg) == 0)
    {
	track_bounce_track = dest_track;
	track_bounce_from_mask = track_mask;
    }
}

void track_key_playstop(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    int i;
    unsigned char track_mask = track_get_track_mask();
    short all_stopped = 1;

    for (i = 1; i <= 8 && all_stopped; i++)
    {
	trackstate = track_current_status >> ((i - 1) * 4);
	trackstate &= 0xf;

	if (trackstate != SPORT_TRACK_STATUS_EMPTY &&
	    trackstate != SPORT_TRACK_STATUS_STOPPED)
	{
	    all_stopped = 0;
	}
    }

    if (!all_stopped)
	track_remember_last_playing();

    /* If all stopped, then play all */
    if (all_stopped)
    {
	track_do_play(track_mask, CMD_SPORT_PLAYNOW, 1);
    }
    else
    {
	for (i = 1; i <= 8; i++)
	{
	    if (!(track_mask & (1 << (i - 1))))
		continue;
	
	    trackstate = track_current_status >> ((i - 1) * 4);
	    trackstate &= 0xf;

	    switch (trackstate)
	    {
	      case SPORT_TRACK_STATUS_EMPTY:
		if (track_current_track == i)
		{
		    ioctl(audiofd, CMD_SPORT_ERASE, 
			  (unsigned long) i | CMD_SPORT_OPTION_FILL);
		}
		break;
	      case SPORT_TRACK_STATUS_PLAYING:
		ioctl(audiofd, 
		      arg ? CMD_SPORT_STOPNOW : CMD_SPORT_STOP, 
		      (unsigned long) i);
		break;
	      case SPORT_TRACK_STATUS_RECORDING:
	      case SPORT_TRACK_STATUS_OVERDUBBING:
	      case SPORT_TRACK_STATUS_STOPPED:
	      case SPORT_TRACK_STATUS_REPLACING:
		ioctl(audiofd, 
		      (arg || all_stopped)? CMD_SPORT_PLAYNOW : CMD_SPORT_PLAY,
		      (unsigned long) i);
		break;
	    }
	}
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "P/S");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_stop(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    int i;
    unsigned char track_mask = track_get_track_mask();

    track_remember_last_playing();

    for (i = 1; i <= 8; i++)
    {
	if (!(track_mask & (1 << (i - 1))))
	    continue;
	
	trackstate = track_current_status >> ((i - 1) * 4);
	trackstate &= 0xf;

	switch (trackstate)
	{
	  case SPORT_TRACK_STATUS_RECORDING:
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	  case SPORT_TRACK_STATUS_PLAYING:
	  case SPORT_TRACK_STATUS_REPLACING:
	    ioctl(audiofd, arg ? CMD_SPORT_STOPNOW : CMD_SPORT_STOP, 
		  (unsigned long) i);
	    break;
	}
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "STOP");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_play(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    int i;
    unsigned char track_mask = track_get_track_mask();
    short all_stopped = 1;

    for (i = 1; i <= 8 && all_stopped; i++)
    {
	trackstate = track_current_status >> ((i - 1) * 4);
	trackstate &= 0xf;

	if (trackstate != SPORT_TRACK_STATUS_EMPTY &&
	    trackstate != SPORT_TRACK_STATUS_STOPPED)
	{
	    all_stopped = 0;
	}
    }

    trackstate = track_current_status >> ((track_current_track - 1) * 4);
    trackstate &= 0xf;

    if (track_current_track > 0 &&
	(track_mask & (1 << (track_current_track - 1))) &&
	trackstate == SPORT_TRACK_STATUS_EMPTY)
    {
	ioctl(audiofd, CMD_SPORT_ERASE, 
	      (unsigned long) track_current_track | CMD_SPORT_OPTION_FILL);
    }
    else if (all_stopped || arg)
    {
	track_do_play(track_mask, CMD_SPORT_PLAYNOW, all_stopped || arg == 2);
    }
    else
    {
	track_do_play(track_mask, CMD_SPORT_PLAY, 0);
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "PLAY");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_do_play(unsigned char track_mask, int play_ioctl, int restart)
{
    unsigned long trackstate;
    unsigned long ioctlarg;
    int longest_track = 0;
    unsigned long longest_track_length = 0;
    int i;

    /* Determine longest track being started */
    if (restart)
    {
	for (i = 1; i <= 8; i++)
	{
	    if (!(track_mask & (1 << (i - 1))))
		continue;
	
	    trackstate = track_current_status >> ((i - 1) * 4);
	    trackstate &= 0xf;

	    switch (trackstate)
	    {
	      case SPORT_TRACK_STATUS_RECORDING:
	      case SPORT_TRACK_STATUS_OVERDUBBING:
	      case SPORT_TRACK_STATUS_STOPPED:
	      case SPORT_TRACK_STATUS_PLAYING:
	      case SPORT_TRACK_STATUS_REPLACING:
		if (last_ls.track_length[i-1] > longest_track_length)
		{
		    longest_track_length = last_ls.track_length[i-1];
		    longest_track = i;
		}
		break;
	    }
	}
    }

    /* Start tracks */
    if (longest_track > 0)
    {
//	printf("play first %d\n", longest_track);
	
	ioctlarg = (unsigned long) longest_track | 0x100;
	restart = 0;
	    
	ioctl(audiofd, play_ioctl, ioctlarg);
    }
    
    for (i = 1; i <= 8; i++)
    {
	if (!(track_mask & (1 << (i - 1))) || i == longest_track)
	    continue;
	
	trackstate = track_current_status >> ((i - 1) * 4);
	trackstate &= 0xf;

	switch (trackstate)
	{
	  case SPORT_TRACK_STATUS_RECORDING:
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	  case SPORT_TRACK_STATUS_STOPPED:
	  case SPORT_TRACK_STATUS_PLAYING:
	  case SPORT_TRACK_STATUS_REPLACING:
	    ioctlarg = (unsigned long) i;
	    if (restart)
		ioctlarg |= 0x100;
	    
	    ioctl(audiofd, play_ioctl, ioctlarg);
	    break;
	}
    }
}

void track_key_allstop(struct menu_s *parent, int displayfd, int arg)
{
    track_remember_last_playing();

    if (arg)
	ioctl(audiofd, CMD_SPORT_STOPNOW, 0);
    else
	ioctl(audiofd, CMD_SPORT_STOP, 0);

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "STOP");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_replaystopall(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    int i;
    short all_stopped = 1;

    for (i = 1; i <= 8 && all_stopped; i++)
    {
	trackstate = track_current_status >> ((i - 1) * 4);
	trackstate &= 0xf;

	if (trackstate != SPORT_TRACK_STATUS_EMPTY &&
	    trackstate != SPORT_TRACK_STATUS_STOPPED)
	{
	    all_stopped = 0;
	}
    }

    if (all_stopped)
    {
	track_do_play(track_last_playing_mask, CMD_SPORT_PLAYNOW, 1);

	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "RP/SA");
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
    else
	track_key_allstop(parent, displayfd, 1);
}

void track_key_allplay(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    int i;
    short all_stopped = 1;

    for (i = 1; i <= 8 && all_stopped; i++)
    {
	trackstate = track_current_status >> ((i - 1) * 4);
	trackstate &= 0xf;

	if (trackstate != SPORT_TRACK_STATUS_EMPTY &&
	    trackstate != SPORT_TRACK_STATUS_STOPPED)
	{
	    all_stopped = 0;
	}
    }

    if (arg)
	track_do_play((unsigned char) 0xff, CMD_SPORT_PLAYNOW, all_stopped);
    else
	track_do_play((unsigned char) 0xff, CMD_SPORT_PLAY, all_stopped);

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "PLAY");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_erase(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    unsigned char track_mask = track_get_track_mask();

    for (i = 1; i <= 8; i++)
    {
	if (!(track_mask & (1 << (i - 1))))
	    continue;
	
	ioctl(audiofd, CMD_SPORT_STOPNOW, (unsigned long) i);
	ioctl(audiofd, CMD_SPORT_ERASE, (unsigned long) i);
    }

    if (track_current_track == 0)
    {
	ioctl(audiofd, CMD_SPORT_SETMASTER, (unsigned long) 0);
	track_current_master = 0;
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "ERASE");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_selecttrack(struct menu_s *parent, int displayfd, int arg)
{
    track_current_track = arg;
    track_everything_except = 0;

    if (track_current_track > 0)
    {
	display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
			      g_current_menu, g_cur_menu_item, 
			      g_display_func, g_display_func_arg );	
	track_level_display = TRACK_DISPLAY_VOLUME;
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
    else
    {
	display_and_menu_set( &all_track_menu, 0, menu_run_display, NULL, 
			      g_current_menu, g_cur_menu_item, 
			      g_display_func, g_display_func_arg );	
	track_level_display = TRACK_DISPLAY_NONE;
    }
}

void track_key_selectgroup(struct menu_s *parent, int displayfd, int arg)
{
    if (arg >= 1 && arg <= 10 && looper_config.track_groups[arg - 1])
    {
	track_current_track = -arg;
	track_everything_except = 0;

	display_and_menu_set( &track_menu, 0, menu_run_display, NULL, g_current_menu, g_cur_menu_item, g_display_func, g_display_func_arg );	
	track_level_display = TRACK_DISPLAY_VOLUME;
    }
}

void track_key_changetrack(struct menu_s *parent, int displayfd, int arg)
{
    int n = track_current_track + arg;

    if (track_current_track <= 0)
    {
	if (arg == -1)
	    n = 8;
	else
	    n = 1;
    }
    else if (n < 1)
	n = 8;
    else if (n > 8)
	n = 1;

    track_key_selecttrack(parent, displayfd, n);
}

void track_change_track_level(int level)
{
    struct sport_track_level_s ld;
    
    ld.track = (short) track_current_track;
    ld.level = (short) level;
    ioctl(audiofd, CMD_SPORT_TRACKLEVEL, &ld);

    looper_config.track_gains[track_current_track - 1] = level;
    track_level_display = TRACK_DISPLAY_VOLUME;
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_change_track_pan(int level)
{
    struct sport_track_level_s ld;
    
    ld.track = (short) track_current_track;
    ld.level = (short) level;
    ioctl(audiofd, CMD_SPORT_PAN, &ld);

    looper_config.track_pans[track_current_track - 1] = level;
    track_level_display = TRACK_DISPLAY_PAN;
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_level(struct menu_s *parent, int displayfd, int arg)
{
    struct sport_track_level_s ld;
    char title[20];
    int level;
    int track;
    unsigned char track_mask = track_get_track_mask();

    if (arg == LP_NO_ARG)
    {
	if (track_current_track <= 0)
	    return;
    
	level = looper_config.track_gains[track_current_track - 1];

	sprintf(title, "Track %d Level", track_current_track);
	if (menu_get_integer(parent, displayfd, title, &level, -128, 128, 
			     track_change_track_level))
	{
	    track_level_deltas[track_current_track - 1] = 0;
	    looper_config.track_gains[track_current_track - 1] = level;
	}
    }
    else
    {
	level = arg;
	if (level > 0)
	    level = 0;
	if (level < -127)
	    level = -127;

	for (track = 1; track <= 8; track++)
	{
	    if (!(track_mask & (1 << (track - 1))))
		continue;

	    track_level_deltas[track - 1] = 0;

	    ld.track = track;
	    ld.level = (short) level;
	    ioctl(audiofd, CMD_SPORT_TRACKLEVEL, &ld);

	    looper_config.track_gains[track - 1] = level;
	    track_level_display = TRACK_DISPLAY_VOLUME;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
	}
    }
}

void track_key_pan(struct menu_s *parent, int displayfd, int arg)
{
    struct sport_track_level_s ld;
    char title[20];
    int level;
    int track;
    unsigned char track_mask = track_get_track_mask();

    if (arg == LP_NO_ARG)
    {
	if (track_current_track <= 0)
	    return;
    
	level = looper_config.track_pans[track_current_track - 1];

	sprintf(title, "Track %d Pan", track_current_track);
	if (menu_get_integer(parent, displayfd, title, &level, -128, 128, 
			     track_change_track_pan))
	{
	    looper_config.track_pans[track_current_track - 1] = level;
	}
    }
    else
    {
	level = arg;
	if (level > 127)
	    level = 127;
	if (level < -127)
	    level = -127;

	for (track = 1; track <= 8; track++)
	{
	    if (!(track_mask & (1 << (track - 1))))
		continue;

	    ld.track = track;
	    ld.level = (short) level;
	    ioctl(audiofd, CMD_SPORT_PAN, &ld);

	    looper_config.track_pans[track - 1] = level;
	    track_level_display = TRACK_DISPLAY_PAN;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
	}
    }
}

void track_change_track_feedback(int feedback)
{
    struct sport_track_level_s ld;
    
    ld.track = (short) track_current_track;
    ld.level = (short) feedback;
    ioctl(audiofd, CMD_SPORT_TRACKFEEDBACK, &ld);

    looper_config.track_feedbacks[track_current_track - 1] = feedback;
    track_level_display = TRACK_DISPLAY_FEEDBACK;
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_feedback(struct menu_s *parent, int displayfd, int arg)
{
    struct sport_track_level_s ld;
    char title[20];
    int feedback;
    short track;
    unsigned char track_mask = track_get_track_mask();

    if (arg == LP_NO_ARG)
    {
	if (track_current_track <= 0)
	    return;
	
	feedback = looper_config.track_feedbacks[track_current_track - 1];

	sprintf(title, "Track %d Feedback", track_current_track);
	if (menu_get_integer(parent, displayfd, title, &feedback, 0, 100, 
			     track_change_track_feedback))
	{
	    looper_config.track_feedbacks[track_current_track - 1] = feedback;
	}
    }
    else if (arg > LP_OFFSET_ARG / 2)
    {
	arg -= LP_OFFSET_ARG;
	
	for (track = 1; track <= 8; track++)
	{
	    if (!(track_mask & (1 << (track - 1))))
		continue;

	    feedback = looper_config.track_feedbacks[track - 1];
	    feedback += arg;
	    if (feedback > 100)
		feedback = 100;
	    if (feedback < 0)
		feedback = 0;

	    ld.track = track;
	    ld.level = (short) feedback;
	    ioctl(audiofd, CMD_SPORT_TRACKFEEDBACK, &ld);

	    looper_config.track_feedbacks[track - 1] = feedback;
	    track_level_display = TRACK_DISPLAY_FEEDBACK;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
	}
    }
    else
    {
	feedback = arg;
	if (feedback > 100)
	    feedback = 100;
	if (feedback < 0)
	    feedback = 0;

	for (track = 1; track <= 8; track++)
	{
	    if (!(track_mask & (1 << (track - 1))))
		continue;

	    ld.track = track;
	    ld.level = (short) feedback;
	    ioctl(audiofd, CMD_SPORT_TRACKFEEDBACK, &ld);

	    looper_config.track_feedbacks[track - 1] = feedback;
	    track_level_display = TRACK_DISPLAY_FEEDBACK;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
	}
    }
}

void track_key_multiply(struct menu_s *parent, int displayfd, int arg)
{
    struct sport_track_level_s ld;
    char title[20];
    int multiple;

    if (track_current_track <= 0)
	return;

    if (arg == 0)
    {
	multiple = 2;

	sprintf(title, "Track %d Multiply", track_current_track);
	if (menu_get_integer(parent, displayfd, title, &multiple, 2, 6, NULL))
	{
	    ld.track = (short) track_current_track;
	    ld.level = (short) multiple;
	    ioctl(audiofd, CMD_SPORT_MULTIPLY, &ld);
	}
    }
    else
    {
	ld.track = (short) track_current_track;
	ld.level = (short) arg;
	ioctl(audiofd, CMD_SPORT_MULTIPLY, &ld);

	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "MULT");
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
}

void track_change_master_level(int level)
{
    ioctl(audiofd, CMD_SPORT_MASTERVOLUME, level);

    looper_config.master_gain = level;
}

void track_key_master_level(struct menu_s *parent, int displayfd, int arg)
{
    char title[20];
    int level;
    
    level = looper_config.master_gain;

    sprintf(title, "Master Gain");
    if (menu_get_integer(parent, displayfd, title, &level, -8, 8, 
			 track_change_master_level))
    {
	config_store();
    }
}

void track_change_beats_per_measure(int bpm)
{
    ioctl(audiofd, CMD_MIDISYNC_BPMEASURE, bpm);

    looper_config.midi_beats_per_measure = bpm;
}

void track_key_beats_per_measure(struct menu_s *parent, int displayfd, int arg)
{
    char title[20];
    int bpm;
    
    bpm = looper_config.midi_beats_per_measure;

    sprintf(title, "Beats / Measure");
    if (menu_get_integer(parent, displayfd, title, &bpm, 1, 100, 
			 track_change_beats_per_measure))
    {
	config_store();
    }
}

void track_change_quantization_steps(int level)
{
    struct sport_track_level_s ld;

    ld.track = track_current_track;
    ld.level = (short) level;
    
    ioctl(audiofd, CMD_SPORT_SETQUANT, &ld);

    looper_config.quantization_steps[track_current_track - 1] = level;
}

void track_key_quantization_steps(struct menu_s *parent, 
				  int displayfd, int arg)
{
    char title[20];
    int level;
    
    if (track_current_track < 1 || track_current_track > 8)
	return;
    
    level = looper_config.quantization_steps[track_current_track - 1];

    sprintf(title, "Quant Steps");
    if (menu_get_integer(parent, displayfd, title, &level, 1, 128, 
			 track_change_quantization_steps))
    {
	config_store();
    }
}

void track_change_volume_min(int level)
{
    looper_config.volume_min = level;
}

void track_key_volume_min(struct menu_s *parent, int displayfd, int arg)
{
    char title[20];
    int level;
    
    level = looper_config.volume_min;

    sprintf(title, "Volume Minimum");
    if (menu_get_integer(parent, displayfd, title, &level, -128, -10, 
			 track_change_volume_min))
    {
	config_store();
    }
}

void track_change_record_level_trigger(int level)
{
    looper_config.record_level_trigger = level;

    ioctl(audiofd, CMD_SPORT_RECORDLEVELTRIGGER, 
	  (unsigned long) looper_config.record_level_trigger);
}

void track_key_record_trigger_level(struct menu_s *parent, int displayfd, 
				    int arg)
{
    char title[20];
    int level;
    
    level = looper_config.record_level_trigger;

    sprintf(title, "Record Trigger");
    if (menu_get_integer(parent, displayfd, title, &level, 0, 23, 
			 track_change_record_level_trigger))
    {
	config_store();
    }
}

void track_change_noisegate_level(int level)
{
    ioctl(audiofd, CMD_SPORT_NOISEGATELVL, 1L << level);

    looper_config.noise_gate_level = level;
}

void track_key_noisegate_level(struct menu_s *parent, int displayfd, int arg)
{
    char title[20];
    int level;
    
    level = looper_config.noise_gate_level;

    sprintf(title, "Noise Gate Level");
    if (menu_get_integer(parent, displayfd, title, &level, 0, 24, 
			 track_change_noisegate_level))
    {
	config_store();
    }
}

void track_change_noisegate_delay(int delay)
{
    ioctl(audiofd, CMD_SPORT_NOISEGATETIME, delay * 10);

    looper_config.noise_gate_delay = delay;
}

void track_key_noisegate_delay(struct menu_s *parent, int displayfd, int arg)
{
    char title[20];
    int delay;
    
    delay = looper_config.noise_gate_delay;

    sprintf(title, "Noise Gate Delay");
    if (menu_get_integer(parent, displayfd, title, &delay, 1, 200, 
			 track_change_noisegate_delay))
    {
	config_store();
    }
}

void track_cc_volume(struct menu_s *parent, int displayfd, int arg)
{
    static short last_cc_vals[8];
    static short last_cc_track = 0;

    struct sport_track_level_s ld;
    short cc;
    short ccval;
    short range;
    long volume;
    short track;
    unsigned char track_mask = track_get_track_mask();
    short i;

    if (track_current_track != 0 && g_current_menu != &track_menu)
    {
	display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
			      g_current_menu, g_cur_menu_item, g_display_func, 
			      g_display_func_arg );	
    }
    else if (track_current_track == 0 && g_current_menu != &all_track_menu)
    {
	display_and_menu_set( &all_track_menu, 0, menu_run_display, NULL, 
			      g_current_menu, g_cur_menu_item, g_display_func, 
			      g_display_func_arg );	
    }

    cc = (arg >> 16) & 0xff;
    ccval = arg & 0xff;

    track = midi_controller_functions[looper_config.controller_func[cc]].arg;

    if (track > 0 && track <= 8)
    {
	track_mask = 1 << (track - 1);

	if (looper_config.cc_track_select && track != track_current_track)
	{
	    track_current_track = track;
	    track_everything_except = 0;
	
	    display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
				  g_current_menu, g_cur_menu_item, 
				  g_display_func, g_display_func_arg );	
	}
    }
    else if (track_current_track > 0)
    {
	if (last_cc_track == 0)
	{
	    for (i = 0; i < 8; i++)
		last_cc_vals[i] = looper_config.controller_max_values[cc];

	    last_cc_track = track_current_track;
	}

	if (track_current_track != last_cc_track &&
	    looper_config.pedal_catch &&
	    (last_cc_vals[track_current_track-1] - ccval < -2 ||
	     last_cc_vals[track_current_track-1] - ccval > 2))
	{
	    track_level_display = TRACK_DISPLAY_VOLUME;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;

	    return;
	}

	last_cc_vals[track_current_track-1] = ccval;
	last_cc_track = track_current_track;
    }

    range = (looper_config.controller_max_values[cc] -
	     looper_config.controller_min_values[cc]);
    ccval -= looper_config.controller_min_values[cc];

    if (ccval == range)
	volume = 0;
    else
    {
	int vmin = looper_config.volume_min;
	
	volume = (((long) ccval * (-vmin)) / range) + vmin;
    }

    for (track = 1; track <= 8; track++)
    {
	if (!(track_mask & (1 << (track - 1))))
	    continue;

	// Stop fade/swell if we set volume
	track_level_deltas[track-1] = 0;

	// Set volume
	ld.track = (short) track;
	ld.level = (short) volume;
	ioctl(audiofd, CMD_SPORT_TRACKLEVEL, &ld);

	looper_config.track_gains[track - 1] = volume;
	track_level_display = TRACK_DISPLAY_VOLUME;
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
}

void track_cc_pan(struct menu_s *parent, int displayfd, int arg)
{
    static short last_cc_vals[8];
    static short last_cc_track = 0;

    struct sport_track_level_s ld;
    short cc;
    short ccval;
    short range;
    long panvalue;
    short track;
    unsigned char track_mask = track_get_track_mask();
    short i;

    if (track_current_track != 0 && g_current_menu != &track_menu)
    {
	display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
			      g_current_menu, g_cur_menu_item, g_display_func, 
			      g_display_func_arg );	
    }
    else if (track_current_track == 0 && g_current_menu != &all_track_menu)
    {
	display_and_menu_set( &all_track_menu, 0, menu_run_display, NULL, 
			      g_current_menu, g_cur_menu_item, g_display_func, 
			      g_display_func_arg );	
    }

    cc = (arg >> 16) & 0xff;
    ccval = arg & 0xff;

    track = midi_controller_functions[looper_config.controller_func[cc]].arg;

    if (track > 0 && track <= 8)
    {
	track_mask = 1 << (track - 1);

	if (looper_config.cc_track_select && track != track_current_track)
	{
	    track_current_track = track;
	    track_everything_except = 0;
	
	    display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
				  g_current_menu, g_cur_menu_item, 
				  g_display_func, g_display_func_arg );	
	}
    }
    else if (track_current_track > 0)
    {
	if (last_cc_track == 0)
	{
	    for (i = 0; i < 8; i++)
		last_cc_vals[i] = looper_config.controller_max_values[cc];

	    last_cc_track = track_current_track;
	}

	if (track_current_track != last_cc_track &&
	    looper_config.pedal_catch &&
	    (last_cc_vals[track_current_track-1] - ccval < -2 ||
	     last_cc_vals[track_current_track-1] - ccval > 2))
	{
	    track_level_display = TRACK_DISPLAY_PAN;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;

	    return;
	}

	last_cc_vals[track_current_track-1] = ccval;
	last_cc_track = track_current_track;
    }

    range = (looper_config.controller_max_values[cc] -
	     looper_config.controller_min_values[cc] + 1);
    ccval -= looper_config.controller_min_values[cc];

    panvalue = ((ccval * 128) / range) - 64;

    for (track = 1; track <= 8; track++)
    {
	if (!(track_mask & (1 << (track - 1))))
	    continue;

	ld.track = (short) track;
	ld.level = (short) panvalue;
	ioctl(audiofd, CMD_SPORT_PAN, &ld);

	looper_config.track_pans[track - 1] = panvalue;
	track_level_display = TRACK_DISPLAY_PAN;
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
}

void track_cc_feedback(struct menu_s *parent, int displayfd, int arg)
{
    static short last_cc_vals[8];
    static short last_cc_track = 0;

    struct sport_track_level_s ld;
    short cc;
    short ccval;
    short range;
    long feedback;
    short track;
    unsigned char track_mask = track_get_track_mask();
    short i;

    if (track_current_track != 0 && g_current_menu != &track_menu)
    {
		display_and_menu_set( &track_menu, 0, menu_run_display, NULL, g_current_menu, g_cur_menu_item, g_display_func, g_display_func_arg );	
    }
    else if (track_current_track == 0 && g_current_menu != &all_track_menu)
    {
		display_and_menu_set( &all_track_menu, 0, menu_run_display, NULL, g_current_menu, g_cur_menu_item, g_display_func, g_display_func_arg );	
    }

    cc = (arg >> 16) & 0xff;
    ccval = arg & 0xff;

    track = midi_controller_functions[looper_config.controller_func[cc]].arg;

    if (track > 0 && track <= 8)
    {
	track_mask = 1 << (track - 1);

	if (looper_config.cc_track_select && track != track_current_track)
	{
	    track_current_track = track;
	    track_everything_except = 0;
	
		display_and_menu_set( &track_menu, 0, menu_run_display, NULL, g_current_menu, g_cur_menu_item, g_display_func, g_display_func_arg );	
	}
    }
    else if (track_current_track > 0)
    {
	if (last_cc_track == 0)
	{
	    for (i = 0; i < 8; i++)
		last_cc_vals[i] = looper_config.controller_max_values[cc];

	    last_cc_track = track_current_track;
	}

	if (track_current_track != last_cc_track &&
	    looper_config.pedal_catch &&
	    (last_cc_vals[track_current_track-1] - ccval < -2 ||
	     last_cc_vals[track_current_track-1] - ccval > 2))
	{
	    track_level_display = TRACK_DISPLAY_FEEDBACK;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;

	    return;
	}

	last_cc_vals[track_current_track-1] = ccval;
	last_cc_track = track_current_track;
    }

    range = (looper_config.controller_max_values[cc] -
	     looper_config.controller_min_values[cc] + 1);
    
    if (ccval == looper_config.controller_max_values[cc])
	feedback = 100;
    else 
	feedback = ((long) ccval * 100) / range;

    if (feedback > 100)
	feedback = 100;
    if (feedback < 0)
	feedback = 0;

    for (track = 1; track <= 8; track++)
    {
	if (!(track_mask & (1 << (track - 1))))
	    continue;

	ld.track = (short) track;
	ld.level = (short) feedback;
	ioctl(audiofd, CMD_SPORT_TRACKFEEDBACK, &ld);

	looper_config.track_feedbacks[track - 1] = feedback;
	track_level_display = TRACK_DISPLAY_FEEDBACK;
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
}

const int pitches[13] = { 0, 6, 12, 19, 26, 33, 41, 50, 59, 68, 78, 89, 100 };

void track_key_mellotron(struct menu_s *parent, int displayfd, int arg)
{
    long l;
    long speed;
    int i;
    unsigned char track_mask = track_get_track_mask();

    if (arg < 0 || arg > 12)
	return;
    
    speed = (long) pitches[arg];

    for( i = 1; i <= 8; i++ )
    {
	if (track_mask & (1 << (i - 1)))
	{
	    l = (speed << 8) | i;
	    ioctl(audiofd, CMD_SPORT_SETSPEED, l);
	}
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy( track_last_cmd, "SPEED" );
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_cc_speed(struct menu_s *parent, int displayfd, int arg)
{
    static short last_cc_vals[8];
    static short last_cc_track = 0;

    short cc;
    short ccval;
    short range;
    long speed;
    long l;
    short track;
    unsigned char track_mask = track_get_track_mask();
    short i;

    if (track_current_track != 0 && g_current_menu != &track_menu)
    {
		display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
				      g_current_menu, g_cur_menu_item, 
				      g_display_func, g_display_func_arg );
    }
    else if (track_current_track == 0 && g_current_menu != &all_track_menu)
    {
		display_and_menu_set( &all_track_menu, 0, menu_run_display, 
				      NULL, g_current_menu, g_cur_menu_item, 
				      g_display_func, g_display_func_arg );
    }

    cc = (arg >> 16) & 0xff;
    ccval = arg & 0xff;

    track = midi_controller_functions[looper_config.controller_func[cc]].arg;

    if (track > 0 && track <= 8)
    {
	track_mask = 1 << (track - 1);

	if (looper_config.cc_track_select && track != track_current_track)
	{
	    track_current_track = track;
	    track_everything_except = 0;
	
		display_and_menu_set( &track_menu, 0, menu_run_display, NULL, 
				      g_current_menu, g_cur_menu_item, 
				      g_display_func, g_display_func_arg );
	}
    }
    else if (track_current_track > 0)
    {
	if (last_cc_track == 0)
	{
	    for (i = 0; i < 8; i++)
		last_cc_vals[i] = looper_config.controller_max_values[cc];

	    last_cc_track = track_current_track;
	}

	if (track_current_track != last_cc_track &&
	    looper_config.pedal_catch &&
	    (last_cc_vals[track_current_track-1] - ccval < -2 ||
	     last_cc_vals[track_current_track-1] - ccval > 2))
	{
	    track_level_display = TRACK_DISPLAY_SPEED;
	    track_display_timeout = TRACK_DISPLAY_TIMEOUT;

	    return;
	}

	last_cc_vals[track_current_track-1] = ccval;
	last_cc_track = track_current_track;
    }

    range = (looper_config.controller_max_values[cc] -
	     looper_config.controller_min_values[cc] + 1);
    
    if (ccval == looper_config.controller_max_values[cc])
	speed = 100;
    else 
	speed = ((long) ccval * 100) / range;

    if (speed > 100)
	speed = 100;
    if (speed < 0)
	speed = 0;

    for (track = 1; track <= 8; track++)
    {
	if (!(track_mask & (1 << (track - 1))))
	    continue;

	l = (speed << 8) | track;
	ioctl(audiofd, CMD_SPORT_SETSPEED, l);

	track_level_display = TRACK_DISPLAY_SPEED;
	track_display_timeout = TRACK_DISPLAY_TIMEOUT;
    }
}

void *track_status_handler(void *arg)
{
    short level_display_timer = 0;
    unsigned char buffer[30];
    struct sport_loop_status_s ls;
    struct sport_track_level_s ld;
    struct track_timer_s tt;
    struct volume_bar_s vb;
    int ledfd = (int) arg;
    short i;
    short lvl;
    short bitmask;
    long l;
    long out_max;
    long in_max;
    short n_recording;
    short vmin;
    
    for (i = 1; i <= 8; i++)
    {
	ld.track = i;
	ld.level = (short) looper_config.track_gains[i - 1];
	
	ioctl(audiofd, CMD_SPORT_TRACKLEVEL, &ld);

	ld.track = i;
	ld.level = (short) looper_config.track_pans[i - 1];
	
	ioctl(audiofd, CMD_SPORT_PAN, &ld);

	ld.track = i;
	ld.level = (short) looper_config.track_feedbacks[i - 1];
	
	ioctl(audiofd, CMD_SPORT_TRACKFEEDBACK, &ld);

	ld.track = i;
	ld.level = (short) looper_config.quantization_steps[i -1];
	
	ioctl(audiofd, CMD_SPORT_SETQUANT, &ld);
    }

    ioctl(audiofd, CMD_SPORT_MASTERVOLUME, looper_config.master_gain);
    ioctl(audiofd, CMD_MIDISYNC_BPMEASURE, 
	  (long) looper_config.midi_beats_per_measure);
    ioctl(audiofd, CMD_SPORT_NOISEGATELVL, 
	  1L << looper_config.noise_gate_level);
    ioctl(audiofd, CMD_SPORT_NOISEGATETIME, looper_config.noise_gate_delay*10);
    ioctl(audiofd, CMD_SPORT_WETONLY, (unsigned long) looper_config.wet_only);
    ioctl(audiofd, CMD_SPORT_VOLUMESPEED, 
	  (unsigned long) looper_config.volume_change_speed);
    ioctl(audiofd, CMD_SPORT_MIDISYNCOUTENABLE, 
	  (unsigned long) looper_config.midi_sync_out_enable);

    out_max = 0;
    in_max = 0;

    while (1)
    {
	vmin = looper_config.volume_min;
	
	/* Fade or swell volumes */
	for (i = 0; i < 8; i++)
	{
	    if (track_level_deltas[i] != 0)
	    {
		if (++track_level_deltacount[i] >= 
		    track_level_deltacountmax[i])
		{
		    track_level_deltacount[i] = 0;
		    
		    if (track_level_deltas[i] > 0 && 
			target_track_levels[i] < looper_config.track_gains[i])
		    {
			track_level_deltas[i] = 0;
		    }
		    else if (track_level_deltas[i] < 0 && 
			     looper_config.track_gains[i] < vmin)
		    {
			looper_config.track_gains[i] = vmin;
			track_level_deltas[i] = 0;
		    }

		    looper_config.track_gains[i] += track_level_deltas[i];

		    ld.track = i + 1;
		    ld.level = (short) looper_config.track_gains[i];
	
		    ioctl(audiofd, CMD_SPORT_TRACKLEVEL, &ld);
		}
	    }
	}

	/* Get new status */
	last_ls = ls;
	
	ioctl(audiofd, CMD_SPORT_LOOPSTATUS, &ls);

	midi_report_period(ls.midi_period);

	track_current_status = ls.track_states;
	track_current_master = ls.master_track;

	if (ls.max_pos_value > in_max)
	    in_max = ls.max_pos_value;
	if (ls.max_out_value > out_max)
	    out_max = ls.max_out_value;

	level_display_timer++;
	if (level_display_timer > 10)
	{
	    level_display_timer = 0;
	    
	    /* Update levels on display */
	    pthread_mutex_lock(&display_lock);

	    if (track_current_track > 0 && g_current_menu == &track_menu)
	    {
		if (track_level_display != TRACK_DISPLAY_POSITION &&
		    track_display_timeout > 0)
		{
		    track_display_timeout--;
		    if (track_display_timeout == 0)
			track_level_display = TRACK_DISPLAY_POSITION;
		}
	    }

	    /*
	      If displaying track position, then now is a good time to
	      update that on the display.
	    */
	    if (track_level_display == TRACK_DISPLAY_POSITION && 
		track_current_track > 0 && 
		g_current_menu == &track_menu &&
		!looper_config.static_display)
	    {
		int loc = (int) (ls.track_position[track_current_track-1] / 
				 35280);
		int rem = (int) (ls.track_length[track_current_track-1] / 
				 35280) - loc;
		
		write(spifd, "\x1f\x24\x36\x00\x00\x00", 6);
		tt.x = 0x36;
		tt.y = 0;

#if DEBUG_DISPLAY_MIDI_ERROR
		if (ls.midi_error[track_current_track-1] != 0)
		{
		    sprintf(buffer, " %ld      ", 
			    ls.midi_error[track_current_track-1] >> 8);
		}
		else 
#endif /* DEBUG_DISPLAY_MIDI_ERROR */

		if (looper_config.time_display == 0 || 
		    ((track_current_status >> ((track_current_track-1) * 4)) 
		     & 0xf) == SPORT_TRACK_STATUS_RECORDING)
		{
		    sprintf(buffer, " +%03d.%d", loc / 10, loc % 10);
		    tt.value = loc;
		}
		else
		{
		    sprintf(buffer, " -%03d.%d", rem / 10, rem % 10);
		    tt.value = -rem;
		}
		
		ioctl(spifd, CMD_SPI_DISPLAY_TRACK_TIMER, &tt);
		write(spifd, buffer, strlen(buffer));
	    }

	    /*
	     * VOLUME bars only if not simple display
	     */
	    if (!looper_config.static_display)
	    {
		/* Select window #2 */
		write(spifd, "\x1f\x28\x77\x01\x02", 5);

		write(spifd, "\x1f\x28\x66\x11\x0c\x00\x02\x00\x01", 9);

		l = 0x80L;
		bitmask = 0;
		for (lvl = 0; lvl < 16; lvl++, l <<= 1)
		    if (in_max > l)
			bitmask |= (1 << lvl);

		vb.in_mask = bitmask;
	    
		in_max = 0;
	
		buffer[0] = (unsigned char) (bitmask >> 8);
		buffer[1] = (unsigned char) bitmask;
		buffer[2] = (unsigned char) (bitmask >> 8);
		buffer[3] = (unsigned char) bitmask;
		buffer[4] = (unsigned char) (bitmask >> 8);
		buffer[5] = (unsigned char) bitmask;

		for (i = 6; i < 12; i++)
		    buffer[i] = (unsigned char) 0;

		l = 0x80L;
		bitmask = 0;
		for (lvl = 0; lvl < 16; lvl++, l <<= 1)
		    if (out_max > l)
			bitmask |= (1 << lvl);
	
		vb.out_mask = bitmask;
		ioctl(spifd, CMD_SPI_DISPLAY_VOLUME_BARS, &vb);

		out_max = 0;

		buffer[12] = (unsigned char) (bitmask >> 8);
		buffer[13] = (unsigned char) bitmask;
		buffer[14] = (unsigned char) (bitmask >> 8);
		buffer[15] = (unsigned char) bitmask;
		buffer[16] = (unsigned char) (bitmask >> 8);
		buffer[17] = (unsigned char) bitmask;

		for (i = 18; i < 24; i++)
		    buffer[i] = (unsigned char) 0;
		write(spifd, buffer, 24);

		/* Select window #1 */
		write(spifd, "\x1f\x28\x77\x01\x01", 5);
	    }

	    pthread_mutex_unlock(&display_lock);
	}

	n_recording = 0;
	for (i = 0; i < 8; i++)
	{
	    switch ((track_current_status >> (i * 4)) & 0xf)
	    {
	      case SPORT_TRACK_STATUS_EMPTY:
		ioctl(ledfd, CMD_SPI_LED_OFF, i + 1);
		break;
	      case SPORT_TRACK_STATUS_PLAYING:
		ioctl(ledfd, CMD_SPI_LED_GREEN, i + 1);
		break;
	      case SPORT_TRACK_STATUS_STOPPED:
		ioctl(ledfd, CMD_SPI_LED_RED, i + 1);
		break;
	      case SPORT_TRACK_STATUS_RECORDING:
		ioctl(ledfd, CMD_SPI_LED_REDFLASH, i + 1);
		n_recording++;
		break;
	      case SPORT_TRACK_STATUS_REPLACING:
	      case SPORT_TRACK_STATUS_OVERDUBBING:
		ioctl(ledfd, CMD_SPI_LED_GREENFLASH, i + 1);
		n_recording++;
		break;
	      case SPORT_TRACK_STATUS_ARMED:
		ioctl(ledfd, CMD_SPI_LED_REDGREENFLASH, i + 1);
		n_recording++;
		break;
	    }
	}

	if (n_recording == 0 && track_bounce_track != 0)
	{
	    track_bounce_track = 0;
	    track_bounce_from_mask = 0;
	}

	usleep(1000);
    }
}

void track_refresh_status(void)
{
    struct sport_loop_status_s ls;

    ioctl(audiofd, CMD_SPORT_LOOPSTATUS, &ls);
    track_current_status = ls.track_states;
    track_current_master = ls.master_track;
}

unsigned char track_get_track_mask(void)
{
    unsigned char mask = 0;
    
    if (track_current_track > 0)
	mask = (unsigned char) (1 << (track_current_track - 1));
    else if (track_current_track == 0)
	mask = '\xff';
    else
	mask = looper_config.track_groups[-1 - track_current_track];

    if (track_everything_except)
    {
	mask ^= 0xff;
    }

    return mask;
}

void track_key_cue(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    unsigned char track_mask = track_get_track_mask();

    for (i = 1; i <= 8; i++)
    {
	if (track_mask & (1 << (i - 1)))
	    ioctl(audiofd, CMD_SPORT_CUE, (unsigned long) i);
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "CUE");
}

void track_key_scramble(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    unsigned char track_mask = track_get_track_mask();

    for (i = 1; i <= 8; i++)
    {
	if (track_mask & (1 << (i - 1)))
	{
	    ioctl(audiofd, CMD_SPORT_SCRAMBLE, 
		  (unsigned long) (i | (arg << 8)));
	}
    }

    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "SCRAM");
}

void track_key_aux(struct menu_s *parent, int displayfd, int arg)
{
    unsigned char track_mask = track_get_track_mask();

    if (arg == 0)
    {
	track_aux1_mask &= ~track_mask;
	track_aux2_mask &= ~track_mask;
    strcpy(track_last_cmd, "MAIN");
    }
    else if (arg == 1)
    {
	track_aux1_mask |= track_mask;
	track_aux2_mask &= ~track_mask;
    strcpy(track_last_cmd, "AUX1");
    }
    else if (arg == 2)
    {
	track_aux1_mask &= ~track_mask;
	track_aux2_mask |= track_mask;
    strcpy(track_last_cmd, "AUX2");
    }
    
    ioctl(audiofd, CMD_SPORT_ASSIGNAUX1, (unsigned long) track_aux1_mask);
    ioctl(audiofd, CMD_SPORT_ASSIGNAUX2, (unsigned long) track_aux2_mask);

    track_level_display = TRACK_DISPLAY_LASTCMD;
}

void track_key_replace(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    long ioctl_arg;

    if (arg == 0)
    {
	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "REPL ");
    }
    else
    {
	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "REPL+");
    }

    if (track_current_track > 0)
    {
	trackstate = track_current_status >> ((track_current_track - 1) * 4);
	trackstate &= 0xf;

	switch (trackstate)
	{
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	  case SPORT_TRACK_STATUS_REPLACING:
	    ioctl(audiofd, CMD_SPORT_PLAY, 
		  (unsigned long) track_current_track);
	    break;
	  case SPORT_TRACK_STATUS_PLAYING:
	  case SPORT_TRACK_STATUS_STOPPED:
	    if (arg > 0)
		ioctl_arg = 0x100 | track_current_track;
	    else
		ioctl_arg = track_current_track;

	    ioctl(audiofd, CMD_SPORT_REPLACE, ioctl_arg);
	    break;
	}
    }
}

void track_key_qreplace(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    long ioctl_arg;

    if (arg == 0)
    {
	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "REPL ");
    }
    else
    {
	track_level_display = TRACK_DISPLAY_LASTCMD;
	strcpy(track_last_cmd, "REPL+");
    }

    if (track_current_track > 0)
    {
	trackstate = track_current_status >> ((track_current_track - 1) * 4);
	trackstate &= 0xf;

	switch (trackstate)
	{
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	  case SPORT_TRACK_STATUS_REPLACING:
	    ioctl(audiofd, CMD_SPORT_PLAY, 
		  (unsigned long) track_current_track);
	    break;
	  case SPORT_TRACK_STATUS_PLAYING:
	  case SPORT_TRACK_STATUS_STOPPED:
	    if (arg > 0)
		ioctl_arg = 0x100 | track_current_track;
	    else
		ioctl_arg = track_current_track;

	    ioctl(audiofd, CMD_SPORT_QREPLACE, ioctl_arg);
	    break;
	}
    }
}

void track_note_replace(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;

    if (track_current_track > 0)
    {
	trackstate = track_current_status >> ((track_current_track - 1) * 4);
	trackstate &= 0xf;

	switch (trackstate)
	{
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	  case SPORT_TRACK_STATUS_REPLACING:
	    if (!arg)
	    {
		ioctl(audiofd, CMD_SPORT_PLAY, 
		      (unsigned long) track_current_track);
	    }
	    break;
	  case SPORT_TRACK_STATUS_PLAYING:
	  case SPORT_TRACK_STATUS_STOPPED:
	    if (arg)
	    {
		ioctl(audiofd, CMD_SPORT_REPLACE, 
		      (unsigned long) track_current_track);
	    }
	    break;
	}
    }
}

void track_note_replaceplus(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long trackstate;
    unsigned long ioctl_arg;

    if (track_current_track > 0)
    {
	trackstate = track_current_status >> ((track_current_track - 1) * 4);
	trackstate &= 0xf;

	switch (trackstate)
	{
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	  case SPORT_TRACK_STATUS_REPLACING:
	    if (!arg)
	    {
		ioctl(audiofd, CMD_SPORT_PLAY, 
		      (unsigned long) track_current_track);
	    }
	    break;
	  case SPORT_TRACK_STATUS_PLAYING:
	  case SPORT_TRACK_STATUS_STOPPED:
	    if (arg)
	    {
		ioctl_arg = 0x100 | track_current_track;
		ioctl(audiofd, CMD_SPORT_REPLACE, ioctl_arg);
	    }
	    break;
	}
    }
}

void track_remember_last_playing(void)
{
    unsigned long trackstate;
    unsigned char track_mask = 0;
    int i;

    for (i = 1; i <= 8; i++)
    {
	trackstate = track_current_status >> ((i - 1) * 4);
	trackstate &= 0xf;

	if (trackstate != SPORT_TRACK_STATUS_EMPTY &&
	    trackstate != SPORT_TRACK_STATUS_STOPPED)
	{
	    track_mask |= (1 << (i - 1));
	}
    }

    if (track_mask)
	track_last_playing_mask = track_mask;
}

void track_key_savelevels(struct menu_s *parent, int displayfd, int arg)
{
    config_save_levels(arg);
    
    track_level_display = TRACK_DISPLAY_LASTCMD;
    strcpy(track_last_cmd, "SAVE");
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

void track_key_restorelevels(struct menu_s *parent, int displayfd, int arg)
{
    int i;
    struct sport_track_level_s ld;
    
    config_restore_levels(arg);
    
    for (i = 1; i <= 8; i++)
    {
	ld.track = (short) i;
	ld.level = (short) looper_config.track_gains[i - 1];
	ioctl(audiofd, CMD_SPORT_TRACKLEVEL, &ld);
	
	ld.track = (short) i;
	ld.level = (short) looper_config.track_pans[i - 1];
	ioctl(audiofd, CMD_SPORT_PAN, &ld);

	ld.track = (short) i;
	ld.level = (short) looper_config.track_feedbacks[i - 1];
	ioctl(audiofd, CMD_SPORT_TRACKFEEDBACK, &ld);
    }

    ioctl(audiofd, CMD_SPORT_MASTERVOLUME, looper_config.master_gain);

    track_level_display = TRACK_DISPLAY_LASTCMD;
    sprintf(track_last_cmd, "PR%02d", arg);
    track_display_timeout = TRACK_DISPLAY_TIMEOUT;
}

