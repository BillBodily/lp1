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
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include "../modules/sport.h"
#include "userif.h"
#include "menus.h"
#include "tracks.h"
#include "config.h"
#include <linux/reboot.h>

struct config_s looper_config;

struct config_functions_s looper_functions[] =
{
    { 47, 47, "Sync Rec/Dub",	track_key_recdub,	2 },
    { 27, 27, "MIDI Sync Rec",	track_key_recdub,	1 },
    { 0, 0, "Rec/Dub",		track_key_recdub,	0 },
    { 60, 60, "Play/Stop Now",	track_key_playstop,	1 },
    { 1, 1, "Play/Stop",	track_key_playstop,	0 },
    { 56, 56, "Replay/All Stop",track_key_replaystopall,0 },
    { 35, 35, "Play Retrigger",	track_key_play,		2 },
    { 13, 13, "Play Now",	track_key_play,		1 },
    { 15, 15, "Play",		track_key_play,		0 },
    { 17, 17, "All Play Now",	track_key_allplay,	1 },
    { 19, 19, "All Play",	track_key_allplay,	0 },
    { 14, 14, "Stop Now",	track_key_stop,		1 },
    { 16, 16, "Stop",		track_key_stop,		0 },
    { 18, 18, "All Stop Now",	track_key_allstop,	1 },
    { 20, 20, "All Stop",	track_key_allstop,	0 },
    { 2, 2, "All Tracks",	track_key_selecttrack,	0 },
    { 3, 3, "Select Track 1",	track_key_selecttrack,	1 },
    { 4, 4, "Select Track 2",	track_key_selecttrack,	2 },
    { 5, 5, "Select Track 3",	track_key_selecttrack,	3 },
    { 6, 6, "Select Track 4",	track_key_selecttrack,	4 },
    { 7, 7, "Select Track 5",	track_key_selecttrack,	5 },
    { 8, 8, "Select Track 6",	track_key_selecttrack,	6 },
    { 9, 9, "Select Track 7",	track_key_selecttrack,	7 },
    { 10, 10, "Select Track 8",	track_key_selecttrack,	8 },
    { 36, 36, "Select Group (1)",	track_key_selectgroup,	1 },
    { 37, 37, "Select Group (2)",	track_key_selectgroup,	2 },
    { 38, 38, "Select Group (3)",	track_key_selectgroup,	3 },
    { 39, 39, "Select Group (4)",	track_key_selectgroup,	4 },
    { 40, 40, "Select Group (5)",	track_key_selectgroup,	5 },
    { 41, 41, "Select Group (6)",	track_key_selectgroup,	6 },
    { 42, 42, "Select Group (7)",	track_key_selectgroup,	7 },
    { 43, 43, "Select Group (8)",	track_key_selectgroup,	8 },
    { 44, 44, "Select Group (9)",	track_key_selectgroup,	9 },
    { 45, 45, "Select Group 10)",	track_key_selectgroup,	10 },
    { 11, 11, "Track Erase",	track_key_erase,	0 },
    { 12, 12, "Track Level",	track_key_level,	LP_NO_ARG },
    { 21, 21, "Set As Master",	track_key_setmaster,	0 },
    { 22, 22, "Next Track", 	track_key_changetrack,	1 },
    { 23, 23, "Prev Track", 	track_key_changetrack,	-1 },
    { 24, 24, "Double", 	track_key_multiply, 	2 },
    { 25, 25, "Triple", 	track_key_multiply, 	3 },
    { 26, 26, "Quadruple", 	track_key_multiply, 	4 },
    { 28, 28, "Reverse Track",	track_key_reverse,	0 },
    { 29, 29, "Half Speed Tr",	track_key_halfspeed,	0 },
    { 30, 30, "Feedback 100%",	track_key_feedback,	100 },
    { 31, 31, "Feedback +10%",	track_key_feedback,	LP_OFFSET_ARG + 10 },
    { 32, 32, "Feedback -10%",	track_key_feedback,	LP_OFFSET_ARG - 10 },
    { 33, 33, "Feedback +5%",	track_key_feedback,	LP_OFFSET_ARG + 5 },
    { 34, 34, "Feedback -5%",	track_key_feedback,	LP_OFFSET_ARG -5 },
    { 46, 46, "Cue Track",	track_key_cue,		0 },
    { 48, 48, "Replace",	track_key_replace,	0 },
    { 49, 49, "Replace+Original",track_key_replace,	1 },
    { 78, 78, "Q Replace",	track_key_qreplace,	0 },
    { 79, 79, "Q Replace+Orig", track_key_qreplace,	1 },
    { 50, 50, "Assign To AUX 1",track_key_aux,		1 },
    { 55, 55, "Assign To AUX 2",track_key_aux,		2 },
    { 62, 62, "Assign To MAIN",	track_key_aux,		0 },
    { 82, 82, "MIDI Stop",	midi_key_stop,		0 },
    { 51, 51, "MIDI Start/Stop",midi_key_startstop,	0 },
    { 52, 52, "Fast Scramble",	track_key_scramble,	1 },
    { 53, 53, "Medium Scramble",track_key_scramble,	4 },
    { 54, 54, "Slow Scramble",	track_key_scramble,	16 },
    { 57, 57, "Bounce",		track_key_bounce,	SPORT_SYNC_NONE },
    { 58, 58, "Sync Bounce",	track_key_bounce,	SPORT_SYNC_INTERNAL },
    { 59, 59, "MIDI Sync Bounce",track_key_bounce,	SPORT_SYNC_MIDI },
    { 61, 61, "MIDI Bypass",	midi_key_bypass,	0 },
    { 63, 63, "Use Preset (0)",	track_key_restorelevels,0 },
    { 64, 64, "Use Preset (1)",	track_key_restorelevels,1 },
    { 65, 65, "Use Preset (2)",	track_key_restorelevels,2 },
    { 66, 66, "Use Preset (3)",	track_key_restorelevels,3 },
    { 67, 67, "Use Preset (4)",	track_key_restorelevels,4 },
    { 68, 68, "Use Preset (5)",	track_key_restorelevels,5 },
    { 69, 69, "Use Preset (6)",	track_key_restorelevels,6 },
    { 70, 70, "Use Preset (7)",	track_key_restorelevels,7 },
    { 71, 71, "Use Preset (8)",	track_key_restorelevels,8 },
    { 72, 72, "Use Preset (9)",	track_key_restorelevels,9 },
    { 73, 73, "Use Preset (10)",track_key_restorelevels,10 },
    { 74, 74, "Pan Center",	track_key_pan,		0 },
    { 77, 77, "Fade/Swell",	track_key_fadeswell,	0 },
    { 76, 76, "Fade",		track_key_fadeswell,	1 },
    { 75, 75, "Swell",		track_key_fadeswell,	2 },
    { 80, 80, "Copy",		track_key_copy,		0 },
    { 81, 81, "Undo",		track_key_undo,		0 },
    { 83, 83, "Octave lower",	track_key_mellotron,	0 },
    { 84, 84, "Minor 2nd",	track_key_mellotron,	1 },
    { 85, 85, "Major 2nd",	track_key_mellotron,	2 },
    { 86, 86, "Minor 3rd",	track_key_mellotron,	3 },
    { 87, 87, "Major 3rd",	track_key_mellotron,	4 },
    { 88, 88, "4th",		track_key_mellotron,	5 },
    { 89, 89, "Diminished 5th",	track_key_mellotron,	6 },
    { 90, 90, "5th",		track_key_mellotron,	7 },
    { 91, 91, "Minor 6th",	track_key_mellotron,	8 },
    { 92, 92, "Major 6th",	track_key_mellotron,	9 },
    { 93, 93, "Minor 7th",	track_key_mellotron,	10 },
    { 94, 94, "Major 7th",	track_key_mellotron,	11 },
    { 95, 95, "Original note",	track_key_mellotron,	12 },
    { 96, 96, "Except Track",	track_key_everythingexcept,	0 },

    { -1, -1, NULL, NULL, 0 }
};

struct config_functions_s midi_controller_functions[] =
{
    { 0, 0, "General Use",	NULL,			0 },
    { 1, 1, "Track Volume",	track_cc_volume,	0 },
    { 2, 2, "Feedback",		track_cc_feedback,	0 },
    { 3, 3, "Track 1 Volume",	track_cc_volume,	1 },
    { 4, 4, "Track 2 Volume",	track_cc_volume,	2 },
    { 5, 5, "Track 3 Volume",	track_cc_volume,	3 },
    { 6, 6, "Track 4 Volume",	track_cc_volume,	4 },
    { 7, 7, "Track 5 Volume",	track_cc_volume,	5 },
    { 8, 8, "Track 6 Volume",	track_cc_volume,	6 },
    { 9, 9, "Track 7 Volume",	track_cc_volume,	7 },
    { 10, 10, "Track 8 Volume",	track_cc_volume,	8 },
    { 11, 11, "Track 1 Feedback",track_cc_feedback,	1 },
    { 12, 12, "Track 2 Feedback",track_cc_feedback,	2 },
    { 13, 13, "Track 3 Feedback",track_cc_feedback,	3 },
    { 14, 14, "Track 4 Feedback",track_cc_feedback,	4 },
    { 15, 15, "Track 5 Feedback",track_cc_feedback,	5 },
    { 16, 16, "Track 6 Feedback",track_cc_feedback,	6 },
    { 17, 17, "Track 7 Feedback",track_cc_feedback,	7 },
    { 18, 18, "Track 8 Feedback",track_cc_feedback,	8 },
    { 19, 19, "Speed",		track_cc_speed,		0 },
    { 20, 20, "Track Pan",track_cc_pan,	0 },
    { 21, 21, "Track 1 Pan",track_cc_pan,	1 },
    { 22, 22, "Track 2 Pan",track_cc_pan,	2 },
    { 23, 23, "Track 3 Pan",track_cc_pan,	3 },
    { 24, 24, "Track 4 Pan",track_cc_pan,	4 },
    { 25, 25, "Track 5 Pan",track_cc_pan,	5 },
    { 26, 26, "Track 6 Pan",track_cc_pan,	6 },
    { 27, 27, "Track 7 Pan",track_cc_pan,	7 },
    { 28, 28, "Track 8 Pan",track_cc_pan,	8 },
};

struct config_functions_s midi_note_functions[] =
{
    { 0, 0, "Replace",		track_note_replace,	0 },
    { 1, 1, "Replace+Original",	track_note_replaceplus,	0 },
};

struct menu_item_s cc_items[] =
{
    { "General Use",	config_select_cc,	NULL, 0 },
    { "Track Volume",	config_select_cc,	NULL, 1 },
    { "Feedback",	config_select_cc,	NULL, 2 },
    { "Track 1 Volume",	config_select_cc,	NULL, 3 },
    { "Track 2 Volume",	config_select_cc,	NULL, 4 },
    { "Track 3 Volume",	config_select_cc,	NULL, 5 },
    { "Track 4 Volume",	config_select_cc,	NULL, 6 },
    { "Track 5 Volume",	config_select_cc,	NULL, 7 },
    { "Track 6 Volume",	config_select_cc,	NULL, 8 },
    { "Track 7 Volume",	config_select_cc,	NULL, 9 },
    { "Track 8 Volume",	config_select_cc,	NULL, 10 },
    { "Track 1 Feedback",config_select_cc,	NULL, 11 },
    { "Track 2 Feedback",config_select_cc,	NULL, 12 },
    { "Track 3 Feedback",config_select_cc,	NULL, 13 },
    { "Track 4 Feedback",config_select_cc,	NULL, 14 },
    { "Track 5 Feedback",config_select_cc,	NULL, 15 },
    { "Track 6 Feedback",config_select_cc,	NULL, 16 },
    { "Track 7 Feedback",config_select_cc,	NULL, 17 },
    { "Track 8 Feedback",config_select_cc,	NULL, 18 },
    { "Speed",		config_select_cc,	NULL, 19 },
    { "Track Pan",config_select_cc,	NULL, 20 },
    { "Track 1 Pan",config_select_cc,	NULL, 21 },
    { "Track 2 Pan",config_select_cc,	NULL, 22 },
    { "Track 3 Pan",config_select_cc,	NULL, 23 },
    { "Track 4 Pan",config_select_cc,	NULL, 24 },
    { "Track 5 Pan",config_select_cc,	NULL, 25 },
    { "Track 6 Pan",config_select_cc,	NULL, 26 },
    { "Track 7 Pan",config_select_cc,	NULL, 27 },
    { "Track 8 Pan",config_select_cc,	NULL, 28 },
};

struct menu_s cc_menu =
{
    "Controller Func",
    NULL,
    &main_menu,
    cc_items,
    sizeof(cc_items) / sizeof(struct menu_item_s),
    NULL
};

static short last_cc_values[128];

extern int reboot(int flag);
extern struct scancode_entry_s scancode_handlers[];

void config_restart(struct menu_s *parent, int displayfd, int arg)
{
    // Display clear
    write(displayfd, "\x0cRestarting...", 14);
    sleep(2);
    reboot(LINUX_REBOOT_CMD_RESTART2);
}

void config_load(void)
{
    char buffer[80];
    FILE *fp;
    int i;
    int n1, n2, n3, n4, n5, n6, n7, n8, n9;

    for (i = 0; i < 8; i++)
    {
	looper_config.track_feedbacks[i] = 100;
	looper_config.track_gains[i] = 0;
	looper_config.track_pans[i] = 0;
	looper_config.track_speeds[i] = 100;
	looper_config.quantization_steps[i] = 1;
    }
    
    for (i = 0; i < 4; i++)
    {
	looper_config.user_buttons[i][0] = -1;
	looper_config.user_buttons[i][1] = -1;
	looper_config.user_buttons[i][2] = -1;
	looper_config.user_buttons[i][3] = -1;
	looper_config.user_buttons[i][4] = -1;
	looper_config.user_buttons[i][5] = -1;
	looper_config.user_buttons[i][6] = -1;
	looper_config.user_buttons[i][7] = -1;
    }
    for (i = 0; i < 512; i++)
    {
	looper_config.midi_pgm_change[i][0] = -1;
	looper_config.midi_pgm_change[i][1] = -1;
	looper_config.midi_pgm_change[i][2] = -1;
	looper_config.midi_pgm_change[i][3] = -1;
	looper_config.midi_pgm_change[i][4] = -1;
	looper_config.midi_pgm_change[i][5] = -1;
	looper_config.midi_pgm_change[i][6] = -1;
	looper_config.midi_pgm_change[i][7] = -1;
    }
    for (i = 0; i < 128; i++)
    {
	last_cc_values[i] = -1;
	looper_config.controller_max_values[i] = 127;
	looper_config.controller_min_values[i] = 0;
	looper_config.controller_func[i] = -1;
	looper_config.note_func[i] = -1;
    }
    for (i = 0; i < 10; i++)
	looper_config.track_groups[i] = 0;

    looper_config.noise_gate_level = 12;
    looper_config.noise_gate_delay = 20;
    looper_config.record_level_trigger = 0;
    looper_config.wet_only = 0;
    looper_config.midi_beats_per_measure = 4;
    looper_config.front_panel_version = 0;
    looper_config.time_display = 0;
    looper_config.static_display = 0;
    looper_config.pedal_catch = 0;
    looper_config.cc_track_select = 0;
    looper_config.midi_sync_out_enable = 0;
    looper_config.volume_change_speed = 0;
    looper_config.volume_fade_time = 10;
    looper_config.volume_min = -128;
    
    fp = fopen("/j/looper.config", "r");
    while (fp != NULL && fgets(buffer, 80, fp) != NULL)
    {
	if (sscanf(buffer, "ipaddr=%lx", &looper_config.ipaddr) == 1)
	    continue;
	if (sscanf(buffer, "gwaddr=%lx", &looper_config.gwaddr) == 1)
	    continue;
	if (sscanf(buffer, "netmask=%lx", &looper_config.netmask) == 1)
	    continue;
	if (sscanf(buffer, "dnsaddr=%lx", &looper_config.dnsaddr) == 1)
	    continue;

 	if (sscanf(buffer, "master_gain=%hd", 
		   &looper_config.master_gain) == 1)
	{
	    continue;
	}
 	if (sscanf(buffer, "volume_min=%hd", 
		   &looper_config.volume_min) == 1)
	{
	    continue;
	}
 	if (sscanf(buffer, "beats_per_measure=%hd", 
		   &looper_config.midi_beats_per_measure) == 1)
	{
	    continue;
	}
	if (sscanf(buffer, "noisegatelevelshift=%hd", 
		   &looper_config.noise_gate_level) == 1)
	{
	    continue;
	}
	if (sscanf(buffer, "noisegatedelay=%hd", 
		   &looper_config.noise_gate_delay) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "midi_channel=%hd", 
		   &looper_config.midi_channel) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "front_panel=%hd", 
		   &looper_config.front_panel_version) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "user%d= %d,%d,%d,%d,%d,%d,%d,%d", 
		   &n1, &n2, &n3, &n4, &n5, &n6, &n7, &n8, &n9) == 9)
	{
	    looper_config.user_buttons[n1-1][0] = n2;
	    looper_config.user_buttons[n1-1][1] = n3;
	    looper_config.user_buttons[n1-1][2] = n4;
	    looper_config.user_buttons[n1-1][3] = n5;
	    looper_config.user_buttons[n1-1][4] = n6;
	    looper_config.user_buttons[n1-1][5] = n7;
	    looper_config.user_buttons[n1-1][6] = n8;
	    looper_config.user_buttons[n1-1][7] = n9;
	    continue;
	}
	else if (sscanf(buffer, "user%d=%d", &n1, &n2) == 2)
	{
	    looper_config.user_buttons[n1-1][0] = n2;
	    continue;
	}

	if (sscanf(buffer, "midipgmchg%d= %d,%d,%d,%d,%d,%d,%d,%d", 
		   &n1, &n2, &n3, &n4, &n5, &n6, &n7, &n8, &n9) == 9)
	{
	    looper_config.midi_pgm_change[n1][0] = n2;
	    looper_config.midi_pgm_change[n1][1] = n3;
	    looper_config.midi_pgm_change[n1][2] = n4;
	    looper_config.midi_pgm_change[n1][3] = n5;
	    looper_config.midi_pgm_change[n1][4] = n6;
	    looper_config.midi_pgm_change[n1][5] = n7;
	    looper_config.midi_pgm_change[n1][6] = n8;
	    looper_config.midi_pgm_change[n1][7] = n9;
	    continue;
	}
	else if (sscanf(buffer, "midipgmchg%d=%d", &n1, &n2) == 2)
	{
	    looper_config.midi_pgm_change[n1][0] = n2;
	    continue;
	}

	if (sscanf(buffer, "midinote%d=%d", &n1, &n2) == 2)
	{
	    looper_config.note_func[n1] = n2;
	    continue;
	}

	if (sscanf(buffer, "midicc%d=%d,%d,%d", &n1, &n2, &n3, &n4) == 4)
	{
	    looper_config.controller_max_values[n1] = n2;
	    looper_config.controller_min_values[n1] = n3;
	    looper_config.controller_func[n1] = n4;
	    continue;
	}

	if (sscanf(buffer, "tracksgroup%d=%x", &n1, &n2) == 2)
	{
	    looper_config.track_groups[n1] = (unsigned char) n2;
	    continue;
	}

	if (sscanf(buffer, "recordleveltrigger=%hd", 
		   &looper_config.record_level_trigger) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "wetonly=%hd", &looper_config.wet_only) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "timedisplay=%hd", 
		   &looper_config.time_display) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "staticdisplay=%hd", 
		   &looper_config.static_display) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "pedalcatch=%hd", 
		   &looper_config.pedal_catch) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "cctrackselect=%hd", 
		   &looper_config.cc_track_select) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "volumechangespeed=%hd", 
		   &looper_config.volume_change_speed) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "volumefadetime=%hd", 
		   &looper_config.volume_fade_time) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, "midisyncoutenable=%hd", 
		   &looper_config.midi_sync_out_enable) == 1)
	{
	    continue;
	}

	if (sscanf(buffer, 
		   "quantization_steps=%hd %hd %hd %hd %hd %hd %hd %hd", 
		   &looper_config.quantization_steps[0],
		   &looper_config.quantization_steps[1],
		   &looper_config.quantization_steps[2],
		   &looper_config.quantization_steps[3],
		   &looper_config.quantization_steps[4],
		   &looper_config.quantization_steps[5],
		   &looper_config.quantization_steps[6],
		   &looper_config.quantization_steps[7]) == 8)
	{
	    continue;
	}
    }

    fclose(fp);
}

void config_store(void)
{
    FILE *fp;
    short i;
    
    fp = fopen("/j/looper.config", "w");
    
    fprintf(fp, "ipaddr=%lx\n", looper_config.ipaddr);
    fprintf(fp, "gwaddr=%lx\n", looper_config.gwaddr);
    fprintf(fp, "netmask=%lx\n", looper_config.netmask);
    fprintf(fp, "dnsaddr=%lx\n", looper_config.dnsaddr);

    for (i = 0; i < 8; i++)
	fprintf(fp, "track%d_gain=%d\n", i + 1, looper_config.track_gains[i]);
    fprintf(fp, "master_gain=%d\n", looper_config.master_gain);
    fprintf(fp, "volume_min=%d\n", looper_config.volume_min);
    fprintf(fp, "beats_per_measure=%d\n", looper_config.midi_beats_per_measure);
    fprintf(fp, "noisegatelevelshift=%d\n", looper_config.noise_gate_level);
    fprintf(fp, "noisegatedelay=%d\n", looper_config.noise_gate_delay);
    fprintf(fp, "wetonly=%d\n", looper_config.wet_only);
    fprintf(fp, "recordleveltrigger=%d\n", looper_config.record_level_trigger);
    fprintf(fp, "timedisplay=%d\n", looper_config.time_display);
    fprintf(fp, "staticdisplay=%d\n", looper_config.static_display);
    fprintf(fp, "pedalcatch=%d\n", looper_config.pedal_catch);
    fprintf(fp, "cctrackselect=%d\n", looper_config.cc_track_select);
    fprintf(fp, "volumechangespeed=%d\n", looper_config.volume_change_speed);
    fprintf(fp, "volumefadetime=%d\n", looper_config.volume_fade_time);
    fprintf(fp, "midisyncoutenable=%d\n", looper_config.midi_sync_out_enable);

    fprintf(fp, "midi_channel=%d\n", looper_config.midi_channel);
    fprintf(fp, "front_panel=%d\n", looper_config.front_panel_version);

    fprintf(fp, "quantization_steps=%d %d %d %d %d %d %d %d\n", 
	    looper_config.quantization_steps[0],
	    looper_config.quantization_steps[1],
	    looper_config.quantization_steps[2],
	    looper_config.quantization_steps[3],
	    looper_config.quantization_steps[4],
	    looper_config.quantization_steps[5],
	    looper_config.quantization_steps[6],
	    looper_config.quantization_steps[7]);

    for (i = 0; i < 4; i++)
    {
	fprintf(fp, "user%d=%d,%d,%d,%d,%d,%d,%d,%d\n", i + 1, 
		looper_config.user_buttons[i][0],
		looper_config.user_buttons[i][1],
		looper_config.user_buttons[i][2],
		looper_config.user_buttons[i][3],
		looper_config.user_buttons[i][4],
		looper_config.user_buttons[i][5],
		looper_config.user_buttons[i][6],
		looper_config.user_buttons[i][7]);
    }
    
    for (i = 0; i < 512; i++)
    {
	if (looper_config.midi_pgm_change[i][0] != -1)
	{
	    fprintf(fp, "midipgmchg%d=%d,%d,%d,%d,%d,%d,%d,%d\n", i,
		    looper_config.midi_pgm_change[i][0],
		    looper_config.midi_pgm_change[i][1],
		    looper_config.midi_pgm_change[i][2],
		    looper_config.midi_pgm_change[i][3],
		    looper_config.midi_pgm_change[i][4],
		    looper_config.midi_pgm_change[i][5],
		    looper_config.midi_pgm_change[i][6],
		    looper_config.midi_pgm_change[i][7]);
	}
    }

    for (i = 0; i < 128; i++)
    {
	if (looper_config.controller_func[i] != -1)
	{
	    fprintf(fp, "midicc%d=%d,%d,%d\n", i,
		    looper_config.controller_max_values[i],
		    looper_config.controller_min_values[i],
		    looper_config.controller_func[i]);
	}
    }

    for (i = 0; i < 128; i++)
    {
	if (looper_config.note_func[i] != -1)
	{
	    fprintf(fp, "midinote%d=%d\n", i,
		    looper_config.note_func[i]);
	}
    }

    for (i = 0; i < 10; i++)
    {
	if (looper_config.track_groups[i])
	{
	    fprintf(fp, "tracksgroup%d=%x\n", 
		    i, looper_config.track_groups[i]);
	}
    }

    fclose(fp);
}
typedef struct _config_address_info
{
	char*			title;
    unsigned long	addr;
    short			current_quad;
	unsigned char	n;
	
} t_config_address_info;

void config_address_display( void* arg, int displayfd )
{
	t_config_address_info* config_address_info = (t_config_address_info*)arg;
	char buffered_line[ 40 ];
    short quad;
	short i;

	strcpy( buffered_line, config_address_info->title );
	strcat( buffered_line, pad_string( buffered_line, 16 ) );

	// Cursor set 0,0
	write( displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );
	write( displayfd, buffered_line, strlen(buffered_line) );

	// Cursor set 0,1
	write( displayfd, "\x1f\x24\x00\x00\x01\x00", 6 );
	for( quad = 3; quad >= 0; quad-- )
	{
	    if( config_address_info->current_quad == quad )
	    {
			i = sprintf( buffered_line, "\x1f\x72\x01%03d\x1f\x72", (unsigned int)config_address_info->n );
			i++;
	    }
	    else
	    {
			i = sprintf( buffered_line, "%03d", (int)((config_address_info->addr >> (8 * quad)) & 0xff) );
	    }
	    
	    write( displayfd, buffered_line, i );

	    if( quad > 0 )
			write( displayfd, ".", 1 );
	}
}

void config_address(struct menu_s *parent, int displayfd, int arg)
{
    short i;
    unsigned long scancode;
	t_config_address_info config_address_info;
	
    switch (arg)
    {
      case 1:	
		config_address_info.addr = looper_config.ipaddr; 
		config_address_info.title = "IP address";
	break;
      case 2:	
		config_address_info.addr = looper_config.gwaddr; 
		config_address_info.title = "Gateway addr";
	break;
      case 3:	
		config_address_info.addr = looper_config.netmask; 
		config_address_info.title = "Netmask";
	break;
      case 4:	
		config_address_info.addr = looper_config.dnsaddr; 
		config_address_info.title = "DNS Server";
	break;
      default: return;
    }

    config_address_info.current_quad = 3;
    config_address_info.n = (unsigned char) (config_address_info.addr >> 24);

	display_and_menu_set( NULL, 0, config_address_display, (void*)&config_address_info, g_current_menu, g_cur_menu_item, menu_run_display, NULL );	

	display_update();

    while (1)
    {
		/* Wait for button press */
		scancode = wait_for_input();

		if ((scancode & 0xff00) == 0x0100)
		{
	    	if (scancode & 0x0080)
				i = -1 - (int) (~scancode & 0xff);
	    	else
				i = (int) (scancode & 0xff);
	    
	    	config_address_info.n += i;
		}
		else if (scancode ==  k_menu_button)		/* MENU button */
		{
			display_and_menu_set( &main_menu, 0, menu_run_display, NULL, &main_menu, 0, menu_run_display, NULL );
	    	return;
		}
		else if (scancode == k_enter_button)		/* ENTER button */
		{
			pthread_mutex_lock( &display_lock );
		    config_address_info.addr &= ~((unsigned long) 0xff << (8 * config_address_info.current_quad));
	    	config_address_info.addr |= ((unsigned long)config_address_info.n << (8 * config_address_info.current_quad));
			pthread_mutex_unlock( &display_lock );
	    
	    	if (config_address_info.current_quad > 0)
	    	{
				pthread_mutex_lock( &display_lock );
				config_address_info.current_quad--;
				config_address_info.n = (unsigned char) (config_address_info.addr >> (config_address_info.current_quad * 8));
				pthread_mutex_unlock( &display_lock );
	    	}
		    else
	    	{
				break;
	    	}
		}
		else if ((scancode & 0xff00) == 0xc000)	/* MIDI button (program change command */
		{
		    short midi_button = (scancode & 0x7f);

	    	config_key_midibutton(parent, displayfd, midi_button);
			display_and_menu_parent();
	    	return;
		}
		else					/* Non-menu button */
		{
		    for (i = 0; scancode_handlers[i].func; i++)
	    	{
				if (scancode == scancode_handlers[i].scancode)
				{
		    		(*scancode_handlers[i].func)(g_current_menu, displayfd, 
						 						 scancode_handlers[i].arg);
		    		return;
				}
	    	}
		}
		
		display_update();
    }

	display_and_menu_parent();

    switch (arg)
    {
      case 1:
		looper_config.ipaddr = config_address_info.addr; 
	  break;
      case 2:	
		looper_config.gwaddr = config_address_info.addr; 
	  break;
      case 3:	
		looper_config.netmask = config_address_info.addr; 
	  break;
      case 4:	
		looper_config.dnsaddr = config_address_info.addr; 
	  break;
    }

    config_store();
}

typedef struct _config_midi_button_info
{
    short current_button;
    short user_button;
    short step;
    short n;
} t_config_midi_button_info;

void config_midi_button_display( void* arg, int displayfd )
{
    t_config_midi_button_info* config_midi_button_info = 
	(t_config_midi_button_info*) arg;
    char buffered_line[ 40 ];

    /* display line 1 */
    if( config_midi_button_info->current_button < 0 && 
	config_midi_button_info->user_button < 0 )
    {
	sprintf( buffered_line, "Press button" );
    }
    else if( config_midi_button_info->current_button >= 0 && 
	     config_midi_button_info->current_button < 128 )
    {
	sprintf( buffered_line, "Pgm Chg %d : %d", 
		 config_midi_button_info->current_button, 
		 config_midi_button_info->step);
    }
    else if( config_midi_button_info->current_button >= 128 && 
	     config_midi_button_info->current_button < 256 )
    {
	sprintf( buffered_line, "CC %d : %d", 
		 config_midi_button_info->current_button - 128, 
		 config_midi_button_info->step);
    }
    else if( config_midi_button_info->current_button >= 256 && 
	     config_midi_button_info->current_button < 384 )
    {
	sprintf( buffered_line, "Note %d : %d", 
		 config_midi_button_info->current_button - 256, 
		 config_midi_button_info->step);
    }
    else
    {
	sprintf( buffered_line, "USER %d : %d", 
		 config_midi_button_info->user_button + 1, 
		 config_midi_button_info->step);
    }
	
    strcat( buffered_line, pad_string( buffered_line, 16 ) );
    write( displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );

    /* display line 2 */
    buffered_line[ 0 ] = 0x00;
    if( config_midi_button_info->current_button >= 0 || 
	config_midi_button_info->user_button >= 0 )
    {
	config_get_function_name(buffered_line, 
				 config_midi_button_info->n);
    }
    strcat( buffered_line, pad_string( buffered_line, 16 ) );
    write( displayfd, "\x1f\x24\x00\x00\x01\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );
}

int config_find_idx(int func_n)
{
    int idx;

    if (func_n < 0)
	return -1;
    
    for (idx = 0; looper_functions[idx].min_id >= 0; idx++)
	if (looper_functions[idx].min_id == func_n)
	    break;
    
    return idx;
}

void config_midi_buttons(struct menu_s *parent, int displayfd, int arg)
{
    short i;
    short n_max;
    unsigned long scancode;
    int cb;
    int step;
	
    t_config_midi_button_info config_midi_button_info;

    n_max = config_get_looper_functions_max();

    config_midi_button_info.current_button = -1;
    config_midi_button_info.user_button = -1;
    config_midi_button_info.step = 1;
    config_midi_button_info.n = 0;

    display_and_menu_set( NULL, 0, config_midi_button_display, 
			  (void*)&config_midi_button_info, g_current_menu, 
			  g_cur_menu_item, menu_run_display, NULL );
    display_update();
	
    while( 1 )
    {
	/* Wait for button press */
	scancode = wait_for_input();
	
	if ((scancode & 0xff00) == 0x0100)
	{
	    if (scancode & 0x0080)
		i = -1 - (int) (~scancode & 0xff);
	    else
		i = (int) (scancode & 0xff);
	    
	    pthread_mutex_lock( &display_lock );
	    config_midi_button_info.n += i;
	    if (config_midi_button_info.n > n_max)
		config_midi_button_info.n = n_max;
	    if (config_midi_button_info.n < -1)
		config_midi_button_info.n = -1;

	    if (config_midi_button_info.current_button >= 0)
	    {
		cb = config_midi_button_info.current_button;
		step = config_midi_button_info.step-1;
		
		if (config_midi_button_info.n >= 0)
		{
		    looper_config.midi_pgm_change[cb][step] = 
			looper_functions[config_midi_button_info.n].min_id;
		}
		else
		{
		    looper_config.midi_pgm_change[cb][step] = 
			config_midi_button_info.n;
		}
	    }
	    else if (config_midi_button_info.user_button >= 0)
	    {
		cb = config_midi_button_info.user_button;
		step = config_midi_button_info.step-1;
		
		if (config_midi_button_info.n >= 0)
		{
		    looper_config.user_buttons[cb][step] = 
			looper_functions[config_midi_button_info.n].min_id;
		}
		else
		{
		    looper_config.user_buttons[cb][step] = 
			config_midi_button_info.n;
		}
	    }
	    
	    pthread_mutex_unlock( &display_lock );
	}
	else if (scancode ==  k_menu_button)		/* MENU button */
	{
	    config_store();

	    display_and_menu_set(&main_menu, 0, menu_run_display, 
				 NULL, &main_menu, 0, menu_run_display, NULL);
	    return;
	}
	else if (scancode == k_enter_button)		/* ENTER button */
	{
	    pthread_mutex_lock( &display_lock );
	    config_midi_button_info.current_button = -1;
	    config_midi_button_info.user_button = -1;
	    config_midi_button_info.step = 1;
	    config_midi_button_info.n = 0;
	    pthread_mutex_unlock( &display_lock );

	    config_store();
	}
	else if ((scancode & 0xff00) == 0xc000)	/* MIDI button */
	{
	    pthread_mutex_lock( &display_lock );

	    config_midi_button_info.current_button = (scancode & 0x7f);
	    config_midi_button_info.user_button = -1;
	    config_midi_button_info.step = 1;

	    cb = config_midi_button_info.current_button;
	    step = config_midi_button_info.step-1;
	    config_midi_button_info.n = 
		config_find_idx(looper_config.midi_pgm_change[cb][step]);

	    pthread_mutex_unlock( &display_lock );
	}
	else if ((scancode & 0xf000) == 0xb000)	/* MIDI controller */
	{
	    pthread_mutex_lock( &display_lock );
	    config_midi_button_info.current_button = 
		((scancode >> 16) & 0x7f) + 128;
	    config_midi_button_info.user_button = -1;
	    config_midi_button_info.step = 1;

	    cb = config_midi_button_info.current_button;
	    step = config_midi_button_info.step-1;
	    config_midi_button_info.n = 
		config_find_idx(looper_config.midi_pgm_change[cb][step]);

	    pthread_mutex_unlock( &display_lock );
	}
	else if ((scancode & 0xff00) == 0x9000)	/* MIDI note on */
	{
	    pthread_mutex_lock( &display_lock );
	    config_midi_button_info.current_button = (scancode & 0x7f) + 256;
	    config_midi_button_info.user_button = -1;
	    config_midi_button_info.step = 1;

	    cb = config_midi_button_info.current_button;
	    step = config_midi_button_info.step-1;
	    config_midi_button_info.n = 
		config_find_idx(looper_config.midi_pgm_change[cb][step]);

	    pthread_mutex_unlock( &display_lock );
	}
	else					/* USER button? */
	{
	    i = -1;
	    
	    pthread_mutex_lock( &display_lock );
	    switch (scancode)
	    {
	      case k_user1_button: i = 0; break;
	      case k_user2_button: i = 1; break;
	      case k_user3_button: i = 2; break;
	      case k_user4_button: i = 3; break;
	      case  k_track1_select_button: 
		config_midi_button_info.step = 1; break;
	      case  k_track2_select_button: 
		config_midi_button_info.step = 2; break;
	      case  k_track3_select_button: 
		config_midi_button_info.step = 3; break;
	      case  k_track4_select_button: 
		config_midi_button_info.step = 4; break;
	      case  k_track5_select_button: 
		config_midi_button_info.step = 5; break;
	      case  k_track6_select_button: 
		config_midi_button_info.step = 6; break;
	      case  k_track7_select_button: 
		config_midi_button_info.step = 7; break;
	      case  k_track8_select_button: 
		config_midi_button_info.step = 8; break;
	    }
	    
	    if (i >= 0)
	    {
		config_midi_button_info.user_button = i;
		config_midi_button_info.current_button = -1;
		config_midi_button_info.step = 1;
	    }
	    
	    if (config_midi_button_info.user_button >= 0)
	    {
		cb = config_midi_button_info.user_button;
		step = config_midi_button_info.step-1;
		config_midi_button_info.n = 
		    config_find_idx(looper_config.user_buttons[cb][step]);
	    }
	    else if (config_midi_button_info.current_button >= 0)
	    {
		cb = config_midi_button_info.current_button;
		step = config_midi_button_info.step-1;
		
		config_midi_button_info.n = 
		    config_find_idx(looper_config.midi_pgm_change[cb][step]);
	    }
	    
	    pthread_mutex_unlock( &display_lock );
	}
	
	display_update();
    }

    config_store();

    display_and_menu_parent();
}

typedef struct _config_midi_notes_info
{
    short current_button;
    short n;
} t_config_midi_notes_info;

void config_midi_notes_display( void* arg, int displayfd )
{
    t_config_midi_notes_info* config_midi_notes_info = 
	(t_config_midi_notes_info*)arg;
    char buffered_line[ 40 ];

    /* display line 1 */
    if( config_midi_notes_info->current_button < 0 )
	sprintf( buffered_line, "Press note" );
    else /* config_midi_notes_info->current_button >= 0 */
	sprintf( buffered_line, "Note %d", 
		 config_midi_notes_info->current_button );

    strcat( buffered_line, pad_string( buffered_line, 16 ) );
    write( displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );

    /* display line 2 */
    buffered_line[ 0 ] = 0x00;
    if( config_midi_notes_info->current_button >= 0 )
    {
	if( config_midi_notes_info->n >= 0 )
	{
	    strcpy( buffered_line, 
		    midi_note_functions[ config_midi_notes_info->n ].name );
	}
	else
	    strcpy( buffered_line, "<not assigned>" );
    }

    strcat( buffered_line, pad_string( buffered_line, 16 ) );
    write( displayfd, "\x1f\x24\x00\x00\x01\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );
}

void config_midi_notes( struct menu_s *parent, int displayfd, int arg )
{
    short i = 0;
    short n_max;
    unsigned long scancode;
    t_config_midi_notes_info config_midi_notes_info;
	
    n_max = sizeof(midi_note_functions) / sizeof(*midi_note_functions) - 1;

    config_midi_notes_info.current_button = -1;
    config_midi_notes_info.n = 0;

    display_and_menu_set( NULL, 0, config_midi_notes_display, 
			  (void*)&config_midi_notes_info, g_current_menu, 
			  g_cur_menu_item, menu_run_display, NULL );	

    display_update();
	
    while( 1 )
    {
	/* Wait for button press */
	scancode = wait_for_input();

	if ((scancode & 0xff00) == 0x0100)
	{
	    if (scancode & 0x0080)
		i = -1 - (int) (~scancode & 0xff);
	    else
		i = (int) (scancode & 0xff);
	    
	    pthread_mutex_lock( &display_lock );
	    config_midi_notes_info.n += i;
	    if (config_midi_notes_info.n > n_max)
		config_midi_notes_info.n = n_max;
	    if (config_midi_notes_info.n < -1)
		config_midi_notes_info.n = -1;

	    if (config_midi_notes_info.current_button >= 0)
	    {
		int btn = config_midi_notes_info.current_button;
		
 		looper_config.note_func[btn] = config_midi_notes_info.n;
		looper_config.midi_pgm_change[btn + 256][0] = -1;
//		config_store();
	    }
	    
	    pthread_mutex_unlock( &display_lock );
	}
	else if (scancode ==  k_menu_button)		/* MENU button */
	{
	    display_and_menu_set( &main_menu, 0, menu_run_display, NULL, 
				  &main_menu, 0, menu_run_display, NULL );
	    return;
	}
	else if (scancode == k_enter_button)		/* ENTER button */
	{
	    pthread_mutex_lock( &display_lock );
	    config_midi_notes_info.current_button = -1;
	    pthread_mutex_unlock( &display_lock );
	    config_store();
	}
	else if ((scancode & 0xff00) == 0x9000)		/* MIDI note */
	{
	    pthread_mutex_lock( &display_lock );
	    config_midi_notes_info.current_button = (scancode & 0x7f);
	    config_midi_notes_info.n = 
		looper_config.note_func[config_midi_notes_info.current_button];
	    pthread_mutex_unlock( &display_lock );
	}
		
	display_update();
    }
}

void config_change_midi_channel(int channel)
{
    looper_config.midi_channel = channel - 1;
}

void config_midi_channel(struct menu_s *parent, int displayfd, int arg)
{
    char title[20];
    int channel;
    
    channel = looper_config.midi_channel + 1;

    sprintf(title, "MIDI channel");
    if (menu_get_integer(parent, displayfd, title, &channel, 1, 16, 
			 config_change_midi_channel))
    {
	config_store();
    }
}

void config_display_ip(struct menu_s *parent, int displayfd, int arg)
{
    char ip[32];
    FILE *fp;
    char *cmd = "ifconfig eth0 | sed -e '/inet addr/pd' | "
	"sed -e 's/^.*inet addr://' | sed -e 's/ .*$//' > /tmp/myip";

    system(cmd);
    
    fp = fopen("/tmp/myip", "r");
    if (fp != NULL &&
	fgets(ip, sizeof(ip), fp) != NULL && 
	strlen(ip) > 0)
    {
	ip[strlen(ip) - 1] = '\0';
    }
    else
	strcpy(ip, "unknown");
    
    menu_display_error(parent, displayfd, "Current IP", ip);
}

static short current_controller = -1;
typedef struct _config_midi_controller_info
{
    short min_value;
    short max_value;
    short state;
} t_config_midi_controller_info;

void config_midi_controller_display( void* arg, int displayfd )
{
    t_config_midi_controller_info* config_midi_controller_info = (t_config_midi_controller_info*)arg;
    char buffered_line[ 40 ];
    short i = 0;

    /* display line 1 */
    if (config_midi_controller_info->state == 0)
	sprintf(buffered_line, "Move controller");
    else if (config_midi_controller_info->state == 1)
	sprintf(buffered_line, "Move %d To Min", current_controller);
    else
	sprintf(buffered_line, "Move %d To Max", current_controller);
    strcat( buffered_line, pad_string( buffered_line, 16 ) );
    write( displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );

    /* display line 2 */
    buffered_line[ 0 ] = 0x00;
    if (config_midi_controller_info->state == 0)
    {
	if (current_controller < 0)
	    i = sprintf(buffered_line, "none selected");
	else
	    i = sprintf(buffered_line, "%d", current_controller);
    }
    else if (config_midi_controller_info->state == 1)
	i = sprintf(buffered_line, "%d", config_midi_controller_info->min_value);
    else if (config_midi_controller_info->state == 2)
	i = sprintf(buffered_line, "%d", config_midi_controller_info->max_value);
    strcat( buffered_line, pad_string( buffered_line, 16 ) );
    write( displayfd, "\x1f\x24\x00\x00\x01\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );
}

void config_midi_controller(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long scancode;
    short i = 0;
    t_config_midi_controller_info config_midi_controller_info;
	
    config_midi_controller_info.min_value = -1;
    config_midi_controller_info.max_value = -1;
    config_midi_controller_info.state = 0;

    display_and_menu_set( NULL, 0, config_midi_controller_display, 
			  (void*)&config_midi_controller_info, g_current_menu, 
			  g_cur_menu_item, menu_run_display, NULL );	

    display_update();
	
    while (1)
    {
	/* Wait for button press */
	scancode = wait_for_input();

	if (scancode ==  k_menu_button)		/* MENU button */
	{
	    display_and_menu_set( &main_menu, 0, menu_run_display, NULL, 
				  &main_menu, 0, menu_run_display, NULL );
	    return;
	}
	else if (scancode == k_enter_button)		/* ENTER button */
	{
	    if (config_midi_controller_info.state == 0 && 
		current_controller >= 0)
	    {
		pthread_mutex_lock( &display_lock );
		config_midi_controller_info.state++;
		pthread_mutex_unlock( &display_lock );
	    }
	    else if (config_midi_controller_info.state == 1 && 
		     config_midi_controller_info.min_value >= 0)
	    {
		pthread_mutex_lock( &display_lock );
		config_midi_controller_info.state++;
		pthread_mutex_unlock( &display_lock );
	    }
	    else if (config_midi_controller_info.state == 2 && 
		     config_midi_controller_info.max_value >= 0)
	    {
		int cur_cc_item;
				
		looper_config.controller_max_values[current_controller] = 
		    config_midi_controller_info.max_value;
		looper_config.controller_min_values[current_controller] = 
		    config_midi_controller_info.min_value;
				
		config_store();

		cur_cc_item = 
		    looper_config.controller_func[current_controller];
		if (cur_cc_item < 0)
		    cur_cc_item = 0;
		else if (cur_cc_item > sizeof(cc_items) / sizeof(cc_items[0]))
		    cur_cc_item = 0;
				
		display_and_menu_set( &cc_menu, cur_cc_item, menu_run_display,
				      NULL, g_parent_menu, g_parent_menu_item, 
				      g_parent_display_func, 
				      g_parent_display_func_arg );
		return;
	    }
	}
	else if ((scancode & 0xf000) == 0xb000)	/* MIDI controller */
	{
	    unsigned short cc = (unsigned short) (scancode >> 16);
	    
	    if (config_midi_controller_info.state == 0)
	    {
		pthread_mutex_lock( &display_lock );
		current_controller = cc;
		pthread_mutex_unlock( &display_lock );
	    }
	    else if (config_midi_controller_info.state == 1 && 
		     cc == current_controller)
	    {
		config_midi_controller_info.min_value = (scancode & 0xff);
	    }
	    else if (config_midi_controller_info.state == 2 && 
		     cc == current_controller)
	    {
		config_midi_controller_info.max_value = (scancode & 0xff);
	    }
	}
	else					/* Non-menu button */
	{
	    for (i = 0; scancode_handlers[i].func; i++)
	    {
		if (scancode == scancode_handlers[i].scancode)
		{
		    (*scancode_handlers[i].func)(g_current_menu, displayfd, 
						 scancode_handlers[i].arg);
		    return;
		}
	    }
	}
		
	display_update();
    }
}

void config_select_cc(struct menu_s *parent, int displayfd, int arg)
{
    looper_config.midi_pgm_change[current_controller + 128][0] = -1;
    looper_config.controller_func[current_controller] = arg;
    config_store();

	display_and_menu_set( parent->parent, 0, menu_run_display, NULL, g_current_menu, g_cur_menu_item, menu_run_display, NULL );	
}

/* arg is the button number */
void config_key_userbutton(struct menu_s *parent, int displayfd, int arg)
{
    short step;
    short idx = 0;

    for (step = 0; step < 8 && idx >= 0; step++)
    {
	idx = looper_config.user_buttons[arg - 1][step];
	if (idx >= 0)
	{
	    config_execute_looper_function(idx, parent, displayfd);
	}
    }
}

void config_key_midiccbutton(struct menu_s *parent, int displayfd, int arg)
{
    int cc;
    int ccval;
    short step;
    short idx = 0;

    cc = ((arg >> 16) & 0x7f);
    ccval = (arg & 0x7f);

    if (last_cc_values[cc] > 64 || ccval < 64)
    {
	last_cc_values[cc] = ccval;
	return;
    }

    last_cc_values[cc] = ccval;

    for (step = 0; step < 8 && idx >= 0; step++)
    {
	idx = looper_config.midi_pgm_change[cc + 128][step];
	if (idx >= 0)
	{
	    config_execute_looper_function(idx, parent, displayfd);
	}
    }
}

void config_key_midibutton(struct menu_s *parent, int displayfd, int arg)
{
    short step;
    short idx = 0;

    for (step = 0; step < 8 && idx >= 0; step++)
    {
	idx = looper_config.midi_pgm_change[arg][step];
	if (idx >= 0)
	{
	    config_execute_looper_function(idx, parent, displayfd);
	}
    }
}

void config_key_noteon(struct menu_s *parent, int displayfd, int arg)
{
    short i;
    
    i = looper_config.note_func[arg];
    
    if (i >= 0)
	(*midi_note_functions[i].func)(g_current_menu, displayfd, 1);
}

void config_key_noteoff(struct menu_s *parent, int displayfd, int arg)
{
    short i;
    
    i = looper_config.note_func[arg];
    
    if (i >= 0)
	(*midi_note_functions[i].func)(g_current_menu, displayfd, 0);
}

void config_set_wetdrymix(struct menu_s *parent, int displayfd, int arg)
{
    looper_config.wet_only = arg;
    config_store();
    
    ioctl(audiofd, CMD_SPORT_WETONLY, (unsigned long) looper_config.wet_only);
}

void config_set_timedisplay(struct menu_s *parent, int displayfd, int arg)
{
    looper_config.time_display = arg;
    config_store();
}

void config_set_staticdisplay(struct menu_s *parent, int displayfd, int arg)
{
    looper_config.static_display = arg;
    config_store();
    display_initialize_display(displayfd);
}

void config_set_pedalcatch(struct menu_s *parent, int displayfd, int arg)
{
    looper_config.pedal_catch = arg;
    config_store();
}

void config_set_cctrackselect(struct menu_s *parent, int displayfd, int arg)
{
    looper_config.cc_track_select = arg;
    config_store();
}

void config_set_volumechangespeed(struct menu_s *parent, int displayfd, 
				  int arg)
{
    looper_config.volume_change_speed = arg;
    ioctl(audiofd, CMD_SPORT_VOLUMESPEED, 
	  (unsigned long) looper_config.volume_change_speed);
    config_store();
}

void config_change_volumefadetime(int n)
{
    looper_config.volume_fade_time = n;
}

void config_set_volumefadetime(struct menu_s *parent, int displayfd, int arg)
{
    char title[20];
    int n;
    
    n = looper_config.volume_fade_time;

    sprintf(title, "Fade Time");
    if (menu_get_integer(parent, displayfd, title, &n, 1, 100, 
			 config_change_volumefadetime))
    {
    }

    config_store();
}

void config_set_midisyncoutenable(struct menu_s *parent, int displayfd, 
				  int arg)
{
    looper_config.midi_sync_out_enable = arg;
    config_store();
    
    ioctl(audiofd, CMD_SPORT_MIDISYNCOUTENABLE, 
	  (unsigned long) looper_config.midi_sync_out_enable);
}

typedef struct _config_group_edit_info
{
	int	group;
    unsigned char track_mask;
	
} t_config_group_edit_info;

void config_group_edit_display( void* arg, int displayfd )
{
	t_config_group_edit_info* config_group_edit_info = (t_config_group_edit_info*)arg;
	char buffered_line[ 40 ];

	/* display line 1 */
    sprintf( buffered_line, "Select Group %d", config_group_edit_info->group );
	strcat( buffered_line, pad_string( buffered_line, 16 ) );
	write( displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );
	write( displayfd, buffered_line, strlen(buffered_line) );

	/* display line 2 */
	sprintf(buffered_line, "%c %c %c %c %c %c %c %c", 
		(config_group_edit_info->track_mask & 0x01) ? '1' : ' ',
		(config_group_edit_info->track_mask & 0x02) ? '2' : ' ',
		(config_group_edit_info->track_mask & 0x04) ? '3' : ' ',
		(config_group_edit_info->track_mask & 0x08) ? '4' : ' ',
		(config_group_edit_info->track_mask & 0x10) ? '5' : ' ',
		(config_group_edit_info->track_mask & 0x20) ? '6' : ' ',
		(config_group_edit_info->track_mask & 0x40) ? '7' : ' ',
		(config_group_edit_info->track_mask & 0x80) ? '8' : ' ');
	strcat( buffered_line, pad_string( buffered_line, 16 ) );
	write( displayfd, "\x1f\x24\x00\x00\x01\x00", 6 );
	write( displayfd, buffered_line, strlen(buffered_line) );
}
void config_group_edit(struct menu_s *parent, int displayfd, int arg)
{
    unsigned long scancode;
    short i;
	t_config_group_edit_info config_group_edit_info;

	config_group_edit_info.group = arg;
    config_group_edit_info.track_mask = looper_config.track_groups[arg-1];

	display_and_menu_set( NULL, 0, config_group_edit_display, (void*)&config_group_edit_info, g_current_menu, g_cur_menu_item, menu_run_display, NULL );	

	display_update();
	
    while (1)
    {
		/* Wait for button press */
		scancode = wait_for_input();

		if (scancode ==  k_menu_button)		/* MENU button */
		{
			display_and_menu_set( &main_menu, 0, menu_run_display, NULL, &main_menu, 0, menu_run_display, NULL );
	    	return;
		}
		else if (scancode == k_enter_button)		/* ENTER button */
		{
		    looper_config.track_groups[arg-1] = config_group_edit_info.track_mask;
	    	config_store();

			display_and_menu_set( parent, arg - 1, menu_run_display, NULL,
			  g_parent_menu, g_parent_menu_item, g_parent_display_func, 
			  g_parent_display_func_arg );
	    	return;
		}
		else					/* Non-menu button */
		{
		    for (i = 0; scancode_handlers[i].func; i++)
	    	{
				if (scancode == scancode_handlers[i].scancode &&
				    scancode_handlers[i].func == track_key_selecttrack &&
		    		scancode_handlers[i].arg > 0)
				{
					pthread_mutex_lock( &display_lock );
				    config_group_edit_info.track_mask ^= (1 << (scancode_handlers[i].arg - 1));
					pthread_mutex_unlock( &display_lock );
				}
	    	}
		}

		display_update();

    }
}

void config_execute_looper_function(int func_n, struct menu_s *parent,
				    int displayfd)
{
    short i;
    
    for (i = 0; looper_functions[i].min_id >= 0; i++)
    {
	if (looper_functions[i].min_id <= func_n &&
	    looper_functions[i].max_id >= func_n)
	{
	    (*looper_functions[i].func)(parent, displayfd, 
					looper_functions[i].arg + func_n - 
					looper_functions[i].min_id);
	    track_refresh_status();
	    return;
	}
    }
}

int config_get_function_name(char *buffer, int idx)
{
    int n = 0;
    
    if (idx < 0)
	n = sprintf(buffer, "end sequence");
    else
	n = sprintf(buffer, "%s", looper_functions[idx].name);
    
    return n;
}

/* --- cpr --- */
/* tiny optimization */
/* this function is called everytime config_midi_buttons() is called. since the 
   looper_functions list is statically built at compile time, it could
   be called once on startup and result stored in a global */
/* if it ever is dynamic the routine that modifies the list would keep the
   global count up to date */
int config_get_looper_functions_max(void)
{
    int i;
    int max_value = 0;
    
    for (i = 0; looper_functions[i].min_id >= 0; i++)
	if (looper_functions[i].max_id > max_value)
	    max_value = looper_functions[i].max_id;
    
    return max_value;
}

int config_save_levels(int preset_num)
{
    char presetname[40];
    FILE *fp;
    
    if (preset_num < 0 || preset_num > 10)
	return -1;
    
    sprintf(presetname, "/j/levelpreset%d", preset_num);
    fp = fopen(presetname, "w");
    
    if (fp == NULL)
	return -1;
    
    fwrite(looper_config.track_gains, sizeof(short), 8, fp);
    fwrite(looper_config.track_feedbacks, sizeof(short), 8, fp);
    fwrite(looper_config.track_pans, sizeof(short), 8, fp);
    fwrite(&looper_config.master_gain, sizeof(short), 1, fp);

    fclose(fp);

    return 0;
}

int config_restore_levels(int preset_num)
{
    char presetname[40];
    FILE *fp;
    
    if (preset_num < 0 || preset_num > 10)
	return -1;
    
    sprintf(presetname, "/j/levelpreset%d", preset_num);
    fp = fopen(presetname, "r");
    
    if (fp == NULL)
	return -1;
    
    fread(looper_config.track_gains, sizeof(short), 8, fp);
    fread(looper_config.track_feedbacks, sizeof(short), 8, fp);
    fread(looper_config.track_pans, sizeof(short), 8, fp);
    fread(&looper_config.master_gain, sizeof(short), 1, fp);

    fclose(fp);

    return 0;
}
