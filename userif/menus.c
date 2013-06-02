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
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include "userif.h"
#include "menus.h"
#include "tracks.h"
#include "config.h"
#include "version.h"

struct menu_item_s upgrade_items[] = 
{
    { "Menu to cancel", ftp_upgrade, NULL, 0 }
};

struct menu_s upgrade_menu =
{
    "Enter to upgrade",
    NULL,
    &main_menu,
    upgrade_items,
    sizeof(upgrade_items) / sizeof(struct menu_item_s),
    NULL
};

struct menu_item_s config_group_items[] = 
{
    { "Group 1", config_group_edit, NULL, 1 },
    { "Group 2", config_group_edit, NULL, 2 },
    { "Group 3", config_group_edit, NULL, 3 },
    { "Group 4", config_group_edit, NULL, 4 },
    { "Group 5", config_group_edit, NULL, 5 },
    { "Group 6", config_group_edit, NULL, 6 },
    { "Group 7", config_group_edit, NULL, 7 },
    { "Group 8", config_group_edit, NULL, 8 },
    { "Group 9", config_group_edit, NULL, 9 },
    { "Group 10", config_group_edit, NULL, 10 }
};

struct menu_s config_group_menu =
{
    "Select group:",
    NULL,
    &main_menu,
    config_group_items,
    sizeof(config_group_items) / sizeof(struct menu_item_s),
    NULL
};

struct menu_item_s midisyncoutenable_items[] = 
{
    { "Off", config_set_midisyncoutenable, NULL, 0 },
    { "On", config_set_midisyncoutenable, NULL, 1 }
};

struct menu_s midisyncoutenable_menu =
{
    "MIDI Clock Out",
    NULL,
    &main_menu,
    midisyncoutenable_items,
    sizeof(midisyncoutenable_items) / sizeof(struct menu_item_s),
    &looper_config.midi_sync_out_enable
};

struct menu_item_s timedisplay_items[] = 
{
    { "Forward", config_set_timedisplay, NULL, 0 },
    { "Reverse", config_set_timedisplay, NULL, 1 }
};

struct menu_s timedisplay_menu =
{
    "Time display",
    NULL,
    &main_menu,
    timedisplay_items,
    sizeof(timedisplay_items) / sizeof(struct menu_item_s),
    &looper_config.time_display
};

struct menu_item_s staticdisplay_items[] = 
{
    { "Off", config_set_staticdisplay, NULL, 0 },
    { "On", config_set_staticdisplay, NULL, 1 }
};

struct menu_s staticdisplay_menu =
{
    "Simple display",
    NULL,
    &main_menu,
    staticdisplay_items,
    sizeof(staticdisplay_items) / sizeof(struct menu_item_s),
    &looper_config.static_display
};

struct menu_item_s pedalcatch_items[] = 
{
    { "Change now", config_set_pedalcatch, NULL, 0 },
    { "Catch first", config_set_pedalcatch, NULL, 1 }
};

struct menu_s pedalcatch_menu =
{
    "Pedal Mode",
    NULL,
    &main_menu,
    pedalcatch_items,
    sizeof(pedalcatch_items) / sizeof(struct menu_item_s),
    &looper_config.pedal_catch
};

struct menu_item_s cctrackselect_items[] = 
{
    { "No", config_set_cctrackselect, NULL, 0 },
    { "Yes", config_set_cctrackselect, NULL, 1 }
};

struct menu_s cctrackselect_menu =
{
    "Track Follows CC",
    NULL,
    &main_menu,
    cctrackselect_items,
    sizeof(cctrackselect_items) / sizeof(struct menu_item_s),
    &looper_config.cc_track_select
};

struct menu_item_s volumechangespeed_items[] = 
{
    { "Very Fast", config_set_volumechangespeed, NULL, 0 },
    { "Fast", config_set_volumechangespeed, NULL, 1 },
    { "Medium", config_set_volumechangespeed, NULL, 2 },
    { "Slow", config_set_volumechangespeed, NULL, 3 },
};

struct menu_s volumechangespeed_menu =
{
    "Vol Change Speed",
    NULL,
    &main_menu,
    volumechangespeed_items,
    sizeof(volumechangespeed_items) / sizeof(struct menu_item_s),
    &looper_config.volume_change_speed
};

struct menu_item_s wetdry_items[] = 
{
    { "Wet + Dry", config_set_wetdrymix, NULL, 0 },
    { "Wet Only", config_set_wetdrymix, NULL, 1 }
};

struct menu_s wetdry_menu =
{
    "Wet/Dry Mix",
    NULL,
    &main_menu,
    wetdry_items,
    sizeof(wetdry_items) / sizeof(struct menu_item_s),
    &looper_config.wet_only
};

struct menu_item_s main_menu_items[] =
{
    { "Version " VERSION_STR, config_display_ip, NULL, 0 },
    { "Record Trigger", track_key_record_trigger_level, NULL, 0 },
    { "Edit Group", NULL, &config_group_menu, 1 },
    { "MIDI channel", config_midi_channel, NULL, 0 },
    { "MIDI/USER btn", config_midi_buttons, NULL, 0 },
    { "Assign Pedal", config_midi_controller, NULL, 0 },
    { "Momentary btn", config_midi_notes, NULL, 0 },
    { "Volume Minimum", track_key_volume_min, NULL, 0 },
    { "Noise Gate Level", track_key_noisegate_level, NULL, 0 },
    { "Noise Gate Delay", track_key_noisegate_delay, NULL, 0 },
    { "Wet/dry mix", NULL, &wetdry_menu, 0 },
    { "Time display", NULL, &timedisplay_menu, 0 },
    { "Simple display", NULL, &staticdisplay_menu, 0 },
    { "Pedal mode", NULL, &pedalcatch_menu, 0 },
    { "Track Follows CC", NULL, &cctrackselect_menu, 0 },
    { "Vol Change Speed", NULL, &volumechangespeed_menu, 0 },
    { "MIDI Clock Out", NULL, &midisyncoutenable_menu, 0 },
    { "Upgrade", NULL, &upgrade_menu, 0 },
    { "IP Address", config_address, NULL, 1 },
    { "Gateway Addr", config_address, NULL, 2 },
    { "Netmask", config_address, NULL, 3 },
    { "DNS Server", config_address, NULL, 4 },
    { "Restart", config_restart, NULL, 0 },
};

struct menu_s main_menu =
{
    "Looperlative LP1",
    NULL,
    &main_menu,
    main_menu_items,
    sizeof(main_menu_items) / sizeof(struct menu_item_s),
    NULL
};

char error_line1[40];
char error_line2[40];

struct menu_item_s error_items[] = 
{
    { error_line2, NULL, NULL, 0 }
};

struct menu_s error_menu =
{
    error_line1, 
    NULL, 
    &error_menu, 
    error_items,
    sizeof(error_items) / sizeof(struct menu_item_s),
    NULL
};

struct scancode_entry_s scancode_handlers[] =
{
    { k_rec_dub_button,  track_key_recdub,		0 },
    { k_play_stop_button, track_key_playstop,	0 },
    { k_track_all_select_button, track_key_selecttrack,	0 },
    { k_track1_select_button, track_key_selecttrack,	1 },
    { k_track2_select_button, track_key_selecttrack,	2 },
    { k_track3_select_button, track_key_selecttrack,	3 },
    { k_track4_select_button, track_key_selecttrack,	4 },
    { k_track5_select_button, track_key_selecttrack,	5 },
    { k_track6_select_button, track_key_selecttrack,	6 },
    { k_track7_select_button, track_key_selecttrack,	7 },
    { k_track8_select_button, track_key_selecttrack,	8 },
    { k_user1_button, config_key_userbutton,	1 },
    { k_user2_button, config_key_userbutton,	2 },
    { k_user3_button, config_key_userbutton,	3 },
    { k_user4_button, config_key_userbutton,	4 },
    { 0x0000, NULL, 0 },
};

struct menu_s*	g_current_menu = NULL;
int 		g_cur_menu_item = 0;
struct menu_s*	g_parent_menu = NULL;
int 		g_parent_menu_item = 0;
int		g_redraw_menu = 0;

void display_initialize_display(int displayfd)
{
    char buffer[20];
    
    write(displayfd, "\x1b\x40\x0c", 3);

    if (!looper_config.static_display)
    {
	/* Create window #1 */
	write(displayfd, "\x1f\x28\x77\x02\x01\x01", 6);
	buffer[0] = 12;	/* x position */
	buffer[1] = 0;
	buffer[2] = 0;	/* y position */
	buffer[3] = 0;
	buffer[4] = 200;/* x size */
	buffer[5] = 0;
	buffer[6] = 2;	/* y size */
	buffer[7] = 0;
	write(displayfd, buffer, 8);

	/* Create window #2 */
	write(displayfd, "\x1f\x28\x77\x02\x02\x01", 6);
	buffer[0] = 0;	/* x position */
	buffer[1] = 0;
	buffer[2] = 0;	/* y position */
	buffer[3] = 0;
	buffer[4] = 12;	/* x size */
	buffer[5] = 0;
	buffer[6] = 2;	/* y size */
	buffer[7] = 0;
	write(displayfd, buffer, 8);

	/* Select window #1 */
	write(displayfd, "\x1f\x28\x77\x01\x01", 5);
	
	/* Set character width */
	write(displayfd, "\x1f\x28\x67\x03\x00", 5);
    }

    g_redraw_menu = 1;
}

void menu_display_error(struct menu_s *menu, int displayfd, 
			char *line1, char *line2)
{
    strncpy(error_line1, line1, 39);
    error_line1[39] = '\0';
    strncpy(error_line2, line2, 39);
    error_line2[39] = '\0';
    error_menu.parent = menu->parent;
    error_items[0].entermenu = menu->parent;
    
    g_current_menu = &error_menu;
    g_cur_menu_item = 0;
}

void menu_init( struct menu_s* menu )
{
    pthread_mutex_lock( &display_lock );
	
    if( menu->default_item_p != NULL )
	g_cur_menu_item = *menu->default_item_p;
    else
	g_cur_menu_item = 0;
    g_current_menu = menu;
	
    pthread_mutex_unlock( &display_lock );
}

#ifdef DEBUG_MENU_SYSTEM
extern struct menu_s track_menu;
extern struct menu_s all_track_menu;
void menu_get_integer_display( void* arg, int displayfd );
void menu_debug_output( char* calling_func_name, 
                        struct menu_s *menu, int menu_item, 
			            t_display_func display_func, void* display_func_arg
                      )
{
	printf( "\n%s\n", calling_func_name );
	
	/* display menu name */
	
	if( menu == NULL )
	{
		printf( "  no menu assigned\n" );
	}
	else 	if( menu == &track_menu )
	{
		printf( "  track_menu\n" );
	}
	else if( menu == &all_track_menu )
	{
		printf( "  all_track_menu\n" );
	}
	else if( menu == &upgrade_menu )
	{
		printf( "  upgrade_menu\n" );
	}
	else if( menu == &config_group_menu )
	{
		printf( "  config_group_menu\n" );
	}
	else if( menu == &midisyncoutenable_menu )
	{
		printf( "  midisyncoutenable_menu\n" );
	}
	else if( menu == &timedisplay_menu )
	{
		printf( "  timedisplay_menu\n" );
	}
	else if( menu == &staticdisplay_menu )
	{
		printf( "  staticdisplay_menu\n" );
	}
	else if( menu == &pedalcatch_menu )
	{
		printf( "  pedalcatch_menu\n" );
	}
	else if( menu == &cctrackselect_menu )
	{
		printf( "  cctrackselect_menu\n" );
	}
	else if( menu == &volumechangespeed_menu )
	{
		printf( "  volumechangespeed_menu\n" );
	}
	else if( menu == &wetdry_menu )
	{
		printf( "  wetdry_menu\n" );
	}
	else if( menu == &main_menu )
	{
		printf( "  main_menu\n" );
	}
	else if( menu == &error_menu )
	{
		printf( "  error_menu\n" );
	}
	else
	{
		printf( "  unknown menu\n" );
	}
		
	/* display menu function */
	if( display_func == menu_run_display )
	{
		printf( "  menu_run_display\n" );
	}
	else if( display_func == menu_get_integer_display )
	{
		printf( "  menu_get_integer_display\n" );
	}
	else
	{
		printf( "  unknown menu display handler\n" );
	}
 }
#endif /* DEBUG_MENU_SYSTEM */
 
void display_and_menu_parent( void )
{
    pthread_mutex_lock( &display_lock );

    g_current_menu  = g_parent_menu;
    g_cur_menu_item = g_parent_menu_item;

    g_display_func 		= g_parent_display_func;
    g_display_func_arg	= g_parent_display_func_arg;

#ifdef DEBUG_MENU_SYSTEM
	menu_debug_output( "display_and_menu_parent", g_current_menu, g_cur_menu_item, g_display_func, g_display_func_arg );
#endif /* DEBUG_MENU_SYSTEM */
	
   pthread_mutex_unlock( &display_lock );
}

void display_and_menu_set( struct menu_s *menu, int menu_item, 
			   t_display_func display_func, void* display_func_arg,
                           struct menu_s *parent_menu, int parent_menu_item, 
			   t_display_func parent_display_func,
			   void* parent_display_func_arg )
{
    pthread_mutex_lock( &display_lock );

    if( parent_menu != NULL )
    {
	g_parent_menu      = parent_menu;
	g_parent_menu_item = parent_menu_item;
	
	g_parent_display_func 	    = parent_display_func;
	g_parent_display_func_arg	= parent_display_func_arg;
    }
    else
    {
	g_parent_menu      = &main_menu;
	g_parent_menu_item = 0;
	
	g_parent_display_func 	    = menu_run_display;
	g_parent_display_func_arg	= NULL;
    }

#ifdef DEBUG_MENU_SYSTEM
    menu_debug_output( "display_and_menu_set - parent", g_parent_menu, g_parent_menu_item, g_parent_display_func, g_parent_display_func_arg );
#endif /* DEBUG_MENU_SYSTEM */

    g_current_menu = menu;
    if( menu_item == -1 )
    {
	if( menu->default_item_p != NULL )
	    g_cur_menu_item = *menu->default_item_p;
	else
	    g_cur_menu_item = 0;
    }
    else
    {
	g_cur_menu_item = menu_item;
    }
	
    g_display_func 		= display_func;
    g_display_func_arg	= display_func_arg;
	
#ifdef DEBUG_MENU_SYSTEM
	menu_debug_output( "display_and_menu_set - current menu", g_current_menu, g_cur_menu_item, g_display_func, g_display_func_arg );
#endif /* DEBUG_MENU_SYSTEM */
	
    pthread_mutex_unlock( &display_lock );
}

void menu_run_display( void* arg, int displayfd )
{
    char buffered_line[60];
    int n;

    // TOP LINE
    if( g_current_menu->titlefunc != NULL )
	strcpy(buffered_line, (*g_current_menu->titlefunc)());
    else
	strcpy(buffered_line, g_current_menu->title);

    strcat(buffered_line, pad_string(buffered_line, 16));
    n = strlen(buffered_line);
    memcpy(buffered_line + n, "\x1f\x72\x00", 3);
    n += 3;

    // Cursor set 0,0
    write(displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );
    write(displayfd, buffered_line, n);

//    printf("1: %s\n", buffered_line);

    // BOTTOM LINE
    if( g_cur_menu_item >= 0 )
    {
	/* Get first value and display it on bottom line */
	struct menu_item_s *item = &g_current_menu->items[ g_cur_menu_item ];
			
	strcpy( buffered_line, item->displaystr );
    }
    else
    {
	strcpy( buffered_line, "none" );
    }
    strcat( buffered_line, pad_string( buffered_line, 16 ) );

    // Cursor set 0,1
    write( displayfd, "\x1f\x24\x00\x00\x01\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );
//    printf("2: %s\n", buffered_line);

    g_redraw_menu = 0;
}

void menu_run( int displayfd )
{
    unsigned long 	scancode;
    int 			i;
    int 			done = 0;
	
    display_and_menu_set( &main_menu, 0, menu_run_display, 
			  NULL, &main_menu, 0, menu_run_display, NULL );
		
    display_update();

    while ( !done )
    {
	/* Wait for button press */
	scancode = wait_for_input();

	struct menu_item_s *item = &g_current_menu->items[ g_cur_menu_item ];

	if( (scancode & 0xff00) == 0x0100 )
	{
	    if( scancode & 0x0080 )
		i = -1 - (int) (~scancode & 0xff);
	    else
		i = (int) (scancode & 0xff);

	    pthread_mutex_lock( &display_lock );
	
	    g_cur_menu_item += i;

	    if( g_cur_menu_item < 0 )
		g_cur_menu_item = 0;
	    else if( g_cur_menu_item >= g_current_menu->n_items )
		g_cur_menu_item = g_current_menu->n_items - 1;
			
	    pthread_mutex_unlock( &display_lock );
	}
	else if( scancode ==  k_menu_button )		/* MENU button */
	{
	    pthread_mutex_lock( &display_lock );
	    display_initialize_display(displayfd);
	    pthread_mutex_unlock( &display_lock );

	    display_and_menu_set( &main_menu, 0, menu_run_display, NULL,
				  &main_menu, 0, menu_run_display, NULL );
	    display_update();
	}
	else if( scancode == k_enter_button )		/* ENTER button */
	{
	    if( item->enterfunc != NULL )
	    {
		(*item->enterfunc)( g_current_menu, displayfd, item->arg );
		if (g_redraw_menu)
		{
		    menu_run_display(NULL, displayfd);
		}
	    }
	    else if( item->entermenu )
	    {
		display_and_menu_set( item->entermenu, -1, menu_run_display, 
				      NULL, g_current_menu, g_cur_menu_item, 
				      menu_run_display, NULL );	
	    }
	    else
	    {
		display_and_menu_parent();
	    }
	}
	else if( (scancode & 0xf000) == 0xb000 )	/* MIDI controller */
	{
	    short i;
	
	    // Is this CC assigned to a button?
	    i = ((scancode >> 16) & 0x7f) + 128;
	    if( looper_config.midi_pgm_change[ i ][ 0 ] >= 0 )
	    {
		config_key_midiccbutton( g_current_menu, displayfd, scancode );
	    }
			
	    // Is this CC assigned to a controller function?
	    else
	    {
		i = looper_config.controller_func[ scancode >> 16 ];
		if (i > 0)
		{
		    (*midi_controller_functions[i].func)(g_current_menu, 
							 displayfd,
							 scancode);
		}
	    }
	}
	else if( (scancode & 0xff00) == 0xc000 )	/* MIDI button */
	{
	    short midi_button = (scancode & 0x7f);
	    config_key_midibutton( g_current_menu, displayfd, midi_button );
	}
	else if( (scancode & 0xff00) == 0x9000 )	/* Note on */
	{
	    short midi_button = (scancode & 0x7f);

	    // Is this note assigned to a button?
	    if( looper_config.midi_pgm_change[midi_button + 256][0] >= 0 )
	    {
		midi_button += 256;
		config_key_midibutton(g_current_menu, displayfd, midi_button);
	    }
	    else
		config_key_noteon(g_current_menu, displayfd, midi_button);
	}
	else if( (scancode & 0xff00) == 0xa000 )	/* Note off */
	{
	    short midi_button = (scancode & 0x7f);

	    if (looper_config.midi_pgm_change[midi_button + 256][0] < 0)
		config_key_noteoff(g_current_menu, displayfd, midi_button);
	}
	else if ((scancode & 0xff00) == 0x0200)		/* UDP command */
	{
	    (*looper_functions[scancode & 0xff].func)(g_current_menu,
						      displayfd,
						      ((int) scancode >> 16));
	}
	else					/* Non-menu button */
	{
	    for( i = 0; scancode_handlers[ i ].func; i++ )
	    {
		if( scancode == scancode_handlers[ i ].scancode )
		{
		    (*scancode_handlers[ i ].func)( g_current_menu, displayfd, 
						    scancode_handlers[i].arg);
		    break;
		}
	    }
	}
		
	display_update();
    }
}

typedef struct _menu_get_integer_info
{
    char* 	title;
    int 	n;
} t_menu_get_integer_info;

void menu_get_integer_display( void* arg, int displayfd )
{
    t_menu_get_integer_info* menu_get_integer_info;
    char buffered_line[60];

    menu_get_integer_info = (t_menu_get_integer_info*)arg;

    strcpy(buffered_line, menu_get_integer_info->title);
    strcat(buffered_line, pad_string( buffered_line, 16));

    // Cursor set 0,0
    write( displayfd, "\x1f\x24\x00\x00\x00\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );

    sprintf( buffered_line, "%d", menu_get_integer_info->n);
    strcat( buffered_line, pad_string( buffered_line, 16 ) );

    // Cursor set 0,1
    write( displayfd, "\x1f\x24\x00\x00\x01\x00", 6 );
    write( displayfd, buffered_line, strlen(buffered_line) );
}

int menu_get_integer(struct menu_s *parent, int displayfd, char *title,
		     int *value, int minval, int maxval,
		     void (*changefunc)(int value))
{
    short i;
    unsigned long scancode;
    t_menu_get_integer_info menu_get_integer_info;
	
    menu_get_integer_info.title = title;
    menu_get_integer_info.n = *value;

    /* check if we are coming from menu_get_integer */
    if( g_display_func != menu_get_integer_display )
    {		
	display_and_menu_set( NULL, 0, menu_get_integer_display,
			      (void*)&menu_get_integer_info, g_current_menu, g_cur_menu_item,
			      g_display_func, g_display_func_arg );
    }

    else
    {
	display_and_menu_set( NULL, 0, menu_get_integer_display,
			      (void*)&menu_get_integer_info, g_parent_menu, g_parent_menu_item,
			      g_parent_display_func, g_parent_display_func_arg );
    }

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
	    
	    pthread_mutex_lock( &display_lock );
	    menu_get_integer_info.n += i;
	    if (menu_get_integer_info.n < minval)
		menu_get_integer_info.n = minval;
	    if (menu_get_integer_info.n > maxval)
		menu_get_integer_info.n = maxval;
	    pthread_mutex_unlock( &display_lock );

	    if (changefunc != NULL)
		(*changefunc)(menu_get_integer_info.n);
	}
	else if (scancode ==  k_menu_button)		/* MENU button */
	{
	    display_and_menu_set( &main_menu, 0, menu_run_display, NULL, &main_menu, 0, menu_run_display, NULL );	
	    return 0;
	}
	else if (scancode == k_enter_button)		/* ENTER button */
	{
	    *value = menu_get_integer_info.n;
			
	    display_and_menu_parent();
	    return 1;
	}
	else if ((scancode & 0xf000) == 0xb000)	/* MIDI controller */
	{
	    long lval;
	    long range;
	    unsigned short cc = (unsigned short) (scancode >> 16);
	    
	    if (looper_config.controller_func[cc] == 0)
	    {
		lval = (long) (scancode & 0xff);
		if (lval < looper_config.controller_min_values[cc])
		    looper_config.controller_min_values[cc] = lval;
		if (lval > looper_config.controller_max_values[cc])
		    looper_config.controller_max_values[cc] = lval;

		range = (looper_config.controller_max_values[cc] -
			 looper_config.controller_min_values[cc]);
		
		pthread_mutex_lock( &display_lock );
		menu_get_integer_info.n = (((maxval - minval) * lval) / range) + minval;
		pthread_mutex_unlock( &display_lock );

		(*changefunc)(menu_get_integer_info.n);
	    }
	}
	else if ((scancode & 0xff00) == 0xc000)	/* MIDI button */
	{
	    struct menu_s*	prev_menu = g_current_menu;
	    short midi_button = (scancode & 0x7f);
	    config_key_midibutton(parent, displayfd, midi_button);

	    /* if the scancode handler did not change to a new menu */
	    /* then we should return to the parent menu */					
	    if( prev_menu == g_current_menu )
	    {
		display_and_menu_parent();
	    }
	    return 0;
	}
	else if ((scancode & 0xff00) == 0x9000)	/* Note on */
	{
	    struct menu_s*	prev_menu = g_current_menu;
	    short midi_button = (scancode & 0x7f);
	    config_key_noteon(parent, displayfd, midi_button);

	    /* if the scancode handler did not change to a new menu */
	    /* then we should return to the parent menu */					
	    if( prev_menu == g_current_menu )
	    {
		display_and_menu_parent();
	    }
	    return 0;
	}
	else if ((scancode & 0xff00) == 0xa000)	/* Note off */
	{
	    struct menu_s*	prev_menu = g_current_menu;
	    short midi_button = (scancode & 0x7f);
	    config_key_noteoff(parent, displayfd, midi_button);

	    /* if the scancode handler did not change to a new menu */
	    /* then we should return to the parent menu */					
	    if( prev_menu == g_current_menu )
	    {
		display_and_menu_parent();
	    }
	    return 0;
	}
	else if ((scancode & 0xff00) == 0x0200)		/* UDP command */
	{
	    (*looper_functions[scancode & 0xff].func)(g_current_menu,
						      displayfd,
						      ((int) scancode >> 16));
	}
	else					/* Non-menu button */
	{
	    for (i = 0; scancode_handlers[i].func; i++)
	    {
		if (scancode == scancode_handlers[i].scancode)
		{
		    struct menu_s*	prev_menu = g_current_menu;
						
		    (*scancode_handlers[i].func)(g_current_menu, displayfd, 
						 scancode_handlers[i].arg);

		    /* if the scancode handler did not change to a new menu */
		    /* then we should return to the parent menu */					
		    if( prev_menu == g_current_menu )
		    {
			display_and_menu_parent();
		    }

		    return 0;
		}
	    }
	}
		
    	display_update();
    }
	
    return( 0 );
}
