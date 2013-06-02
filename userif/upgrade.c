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
#include <linux/reboot.h>
#include "userif.h"
#include "menus.h"

extern int reboot(int flag);

void ftp_upgrade(struct menu_s *parent, int displayfd, int arg)
{
    // Clear bottom line
    write(displayfd, "\x1f\x72\x00", 3);
    write(displayfd, "\x1f\x24\x00\x00\x01\x00", 6);
    write(displayfd, "                ", 16);

    // Write top line
    write(displayfd,"\x1f\x24\x00\x00\x00\x00", 6);
    write(displayfd, "Downloading...  ", 16);

    /* Download upgrade from Looperlative home server */
    chdir("/tmp");
    if (system("/bin/ftpget -u looperupgrade -p vf605pk "
	       "upgrade.looperlative.com lp1upgrade.tar lp1upgrade.tar"
	       "> /dev/null 2>&1") != 0)
    {
	 menu_display_error(parent, displayfd, 
			    "Upgrade Error", "Download failed");
	 return;
    }

    write(displayfd,"\x1f\x24\x00\x00\x00\x00", 6);
    write(displayfd, "Unpacking...    ", 16);

    /* We have the upgrade, now unpack it. */
    if (system("tar xf lp1upgrade.tar > /dev/null 2>&1") != 0)
    {
	menu_display_error(parent, displayfd, 
			   "Upgrade Error", "Unpack failed");
	return;
    }
    
    write(displayfd,"\x1f\x24\x00\x00\x00\x00", 6);
    write(displayfd, "Upgrading...    ", 16);

    /* Run the upgrade */
    if (system("sh upgrade.sh > /dev/console 2>&1") != 0)
    {
	menu_display_error(parent, displayfd, 
			   "Upgrade Error", "Upgrade failed");
	return;
    }

    /* Reset the system */
    write(displayfd,"\x1f\x24\x00\x00\x00\x00", 6);
    write(displayfd, "Upgrade Complete", 16);
    write(displayfd, "\x1f\x24\x00\x00\x01\x00Please reboot", 19);
    sleep(2);
//    reboot(LINUX_REBOOT_CMD_RESTART2);
    while (1);
}
