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

#ifndef __CONFIG_H__
#define __CONFIG_H__

struct config_s
{
    /* Networking configuration */
    unsigned long ipaddr;
    unsigned long gwaddr;
    unsigned long netmask;
    unsigned long dnsaddr;
    
    /* Volume configuration */
    short track_gains[8];
    short track_feedbacks[8];
    short track_speeds[8];
    short track_pans[8];
    short master_gain;

    short noise_gate_level;
    short noise_gate_delay;

    short record_level_trigger;

    short wet_only;

    /* MIDI configuration */
    short midi_channel;
    short midi_pgm_change[512][8];
    short controller_max_values[128];
    short controller_min_values[128];
    short controller_func[128];
    short note_func[128];
    short midi_beats_per_measure;
    short midi_sync_out_enable;

    /* Configurable buttons */
    short user_buttons[4][8];

    /* Groups */
    unsigned char track_groups[10];

    /* Electronics revision */
    short front_panel_version;

    /* Miscellaneous */
    short time_display;
    short static_display;
    short pedal_catch;
    short cc_track_select;
    short volume_change_speed;
    short volume_fade_time;
    short volume_min;
    short quantization_steps[8];
};

struct config_functions_s
{
    int min_id;
    int max_id;
    char *name;
    void (*func)(struct menu_s *parent, int displayfd, int arg);
    int arg;
};

#define LP_NO_ARG		30000
#define LP_OFFSET_ARG		20000

extern struct config_s looper_config;
extern struct config_functions_s looper_functions[];
extern struct config_functions_s midi_controller_functions[];

void config_restart(struct menu_s *parent, int displayfd, int arg);
void config_load(void);
void config_store(void);
void config_address(struct menu_s *parent, int displayfd, int arg);
void config_user_buttons(struct menu_s *parent, int displayfd, int arg);
void config_midi_buttons(struct menu_s *parent, int displayfd, int arg);
void config_midi_channel(struct menu_s *parent, int displayfd, int arg);
void config_midi_controller(struct menu_s *parent, int displayfd, int arg);
void config_key_userbutton(struct menu_s *parent, int displayfd, int arg);
void config_select_cc(struct menu_s *parent, int displayfd, int arg);
void config_set_wetdrymix(struct menu_s *parent, int displayfd, int arg);
void config_group_edit(struct menu_s *parent, int displayfd, int arg);
void config_execute_looper_function(int func_n, struct menu_s *parent,
				    int displayfd);
int config_get_function_name(char *buffer, int func_n);
int config_get_looper_functions_max(void);
void config_key_midibutton(struct menu_s *parent, int displayfd, int arg);
void config_key_noteon(struct menu_s *parent, int displayfd, int arg);
void config_key_noteoff(struct menu_s *parent, int displayfd, int arg);
void config_midi_notes(struct menu_s *parent, int displayfd, int arg);
void config_set_timedisplay(struct menu_s *parent, int displayfd, int arg);
void config_set_staticdisplay(struct menu_s *parent, int displayfd, int arg);
void config_set_pedalcatch(struct menu_s *parent, int displayfd, int arg);
void config_set_cctrackselect(struct menu_s *parent, int displayfd, int arg);
void config_set_midisyncoutenable(struct menu_s *parent, int displayfd, 
				  int arg);
void config_set_volumechangespeed(struct menu_s *parent, int displayfd, 
				  int arg);
void config_set_volumefadetime(struct menu_s *parent, int displayfd, int arg);
void config_display_ip(struct menu_s *parent, int displayfd, int arg);

void midi_key_bypass(struct menu_s *parent, int displayfd, int arg);
void config_key_midiccbutton(struct menu_s *parent, int displayfd, int arg);
int config_save_levels(int preset_num);
int config_restore_levels(int preset_num);

#endif /* __CONFIG_H__ */
