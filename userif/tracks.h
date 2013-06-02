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

#ifndef __TRACKS_H__
#define __TRACKS_H__

void track_key_savetrackaudio(struct menu_s *parent, int displayfd, int arg);
void track_key_loadtrackaudio(struct menu_s *parent, int displayfd, int arg);
void track_key_recdub(struct menu_s *parent, int displayfd, int arg);
void track_key_playstop(struct menu_s *parent, int displayfd, int arg);
void track_key_selecttrack(struct menu_s *parent, int displayfd, int arg);
void track_key_selectgroup(struct menu_s *parent, int displayfd, int arg);
void track_key_erase(struct menu_s *parent, int displayfd, int arg);
void track_key_level(struct menu_s *parent, int displayfd, int arg);
void track_key_feedback(struct menu_s *parent, int displayfd, int arg);
void track_key_play(struct menu_s *parent, int displayfd, int arg);
void track_key_stop(struct menu_s *parent, int displayfd, int arg);
void track_key_allplay(struct menu_s *parent, int displayfd, int arg);
void track_key_allstop(struct menu_s *parent, int displayfd, int arg);
void track_key_setmaster(struct menu_s *parent, int displayfd, int arg);
void track_key_changetrack(struct menu_s *parent, int displayfd, int arg);
void track_key_master_level(struct menu_s *parent, int displayfd, int arg);
void track_key_noisegate_level(struct menu_s *parent, int displayfd, int arg);
void track_key_noisegate_delay(struct menu_s *parent, int displayfd, int arg);
void track_key_record_trigger_level(struct menu_s *parent, int displayfd, 
				    int arg);
void track_cc_volume(struct menu_s *parent, int displayfd, int arg);
void track_cc_pan(struct menu_s *parent, int displayfd, int arg);
void track_cc_feedback(struct menu_s *parent, int displayfd, int arg);
void track_change_track_level(int level);
void track_key_multiply(struct menu_s *parent, int displayfd, int arg);
void track_key_reverse(struct menu_s *parent, int displayfd, int arg);
void track_key_halfspeed(struct menu_s *parent, int displayfd, int arg);
void track_key_beats_per_measure(struct menu_s *parent, int displayfd, 
				 int arg);
void track_key_cue(struct menu_s *parent, int displayfd, int arg);
unsigned char track_get_track_mask(void);
void track_refresh_status(void);
void track_key_replace(struct menu_s *parent, int displayfd, int arg);
void track_key_qreplace(struct menu_s *parent, int displayfd, int arg);
void track_note_replace(struct menu_s *parent, int displayfd, int arg);
void track_note_replaceplus(struct menu_s *parent, int displayfd, int arg);
void track_key_aux(struct menu_s *parent, int displayfd, int arg);
void track_key_scramble(struct menu_s *parent, int displayfd, int arg);
void track_cc_speed(struct menu_s *parent, int displayfd, int arg);
void track_do_play(unsigned char track_mask, int play_ioctl, int restart);
void track_remember_last_playing(void);
void track_key_replaystopall(struct menu_s *parent, int displayfd, int arg);
void track_key_bounce(struct menu_s *parent, int displayfd, int arg);
void track_key_pan(struct menu_s *parent, int displayfd, int arg);
void track_key_savelevels(struct menu_s *parent, int displayfd, int arg);
void track_key_restorelevels(struct menu_s *parent, int displayfd, int arg);
void track_key_fadeswell(struct menu_s *parent, int displayfd, int arg);
void track_key_quantization_steps(struct menu_s *parent, 
				  int displayfd, int arg);
void track_key_volume_min(struct menu_s *parent, int displayfd, int arg);
void track_key_undo(struct menu_s *parent, int displayfd, int arg);
void track_key_copy(struct menu_s *parent, int displayfd, int arg);
void track_key_mellotron(struct menu_s *parent, int displayfd, int arg);
void track_key_everythingexcept(struct menu_s *parent, int displayfd, int arg);

#endif /* __TRACKS_H__ */
