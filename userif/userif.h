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

#ifndef __USERIF_H__
#define __USERIF_H__

struct menu_s;
	
#define k_spaces_for_padding	"                                "

typedef void (*t_display_func)( void* arg, int displayfd );
void display_update( void );

/* scan codes for front panel buttons */
#define k_user1_button				0x0810
#define k_user2_button				0x0820
#define k_rec_dub_button			0x0840
#define k_menu_button				0x0880
#define k_user3_button				0x1010
#define k_user4_button				0x1020
#define k_play_stop_button			0x1040
#define k_enter_button	0x1080
#define k_track1_select_button		0x2080
#define k_track2_select_button		0x2040
#define k_track3_select_button		0x2010
#define k_track4_select_button		0x2020
#define k_track5_select_button		0x4080
#define k_track6_select_button		0x4040
#define k_track7_select_button		0x4010
#define k_track8_select_button		0x4020
#define k_track_all_select_button	0x8080

unsigned long wait_for_input(void);
void ftp_upgrade(struct menu_s *parent, int displayfd, int arg);
void midi_key_startstop(struct menu_s *parent, int displayfd, int arg);
void midi_key_stop(struct menu_s *parent, int displayfd, int arg);

extern pthread_mutex_t display_lock;
extern int audiofd;
extern t_display_func 	g_display_func;
extern void* 			g_display_func_arg;
extern t_display_func 	g_parent_display_func;
extern void* 			g_parent_display_func_arg;

static inline char* pad_string( char* string_to_pad, int pad_width )
{
	/* first we point at the end of the string */
	char* padding = k_spaces_for_padding + strlen( k_spaces_for_padding );
	
	/* if our padding string is long enough to supply the max padding */
	/* AND the length of the string to pad is less than, or equal to the required padding */
	if( (pad_width <= strlen( k_spaces_for_padding )) && (strlen( string_to_pad ) < pad_width ) )
	{
		padding = padding -	pad_width + strlen( string_to_pad );
	}
	
	/* return a pointer to the required padding */
	return( padding );
}
#endif /* __USERIF_H__ */
