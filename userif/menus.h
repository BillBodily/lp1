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

#ifndef __MENUS_H__
#define __MENUS_H__

struct menu_s
{
    char *title;
    char *(*titlefunc)(void);
    struct menu_s *parent;
    struct menu_item_s *items;
    int n_items;
    short *default_item_p;
};

struct menu_item_s
{
    char *displaystr;
    void (*enterfunc)(struct menu_s *parent, int displayfd, int arg);
    struct menu_s *entermenu;
    int arg;
};

struct scancode_entry_s
{
    unsigned short scancode;
    void (*func)(struct menu_s *parent, int displayfd, int arg);
    int arg;
};

extern struct menu_s main_menu;
	
extern struct menu_s*	g_current_menu;
extern int 				g_cur_menu_item;
extern struct menu_s*	g_parent_menu;
extern int 				g_parent_menu_item;

void menu_run(int displayfd);
void menu_run_display( void* arg, int displayfd );
void menu_display_error(struct menu_s *menu, int displayfd, 
			char *line1, char *line2);
int menu_get_integer(struct menu_s *parent, int displayfd, char *title,
		     int *value, int minval, int maxval,
		     void (*changefunc)(int value));

void display_and_menu_parent( void );
void display_and_menu_set( struct menu_s *menu, int item, t_display_func display_func, void* display_func_arg,
                           struct menu_s *parent_menu, int parent_menu_item, t_display_func parent_display_func,
						   void* parent_display_func_arg );

#endif /* __MENUS_H__ */
