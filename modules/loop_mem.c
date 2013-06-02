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

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/param.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <asm/blackfin.h>

#include "loop_mem.h"

static short free_list[LOOPER_MAX_BLOCKS];
static short free_list_start = 0;
static short free_list_end = 0;

static spinlock_t looper_mem_lock = SPIN_LOCK_UNLOCKED;

void looper_mem_init(void)
{
    int i;
    
    for (i = 0; i < LOOPER_MAX_BLOCKS; i++)
	free_list[i] = i;

    free_list_start = 0;
    free_list_end = LOOPER_MAX_BLOCKS - 1;

    memset((void *) (32 * 1024 * 1024), 0, 
	   LOOPER_MAX_BLOCKS * LOOPER_BLOCK_SIZE);
}

long *looper_alloc(void)
{
    int block;
    unsigned long addr;
    unsigned long flags;
    
    spin_lock_irqsave(&looper_mem_lock, flags);

    if (free_list_start == free_list_end)
	addr = 0;
    else
    {
	block = free_list[free_list_start];
	free_list[free_list_start] = -1;

	free_list_start++;
	if (free_list_start >= LOOPER_MAX_BLOCKS)
	    free_list_start = 0;
	
	addr = (32 * 1024 * 1024) + (block * LOOPER_BLOCK_SIZE);
    }

    spin_unlock_irqrestore(&looper_mem_lock, flags);

    return (long *) addr;
}

void looper_free(long *blockaddr)
{
    unsigned long flags;
    int block;
    int offset;
    short next;
    
    offset = (int) blockaddr - (32 * 1024 * 1024);
    if (offset % LOOPER_BLOCK_SIZE)
    {
/*	printk("looper_free: bad argument.\n"); */
	return;
    }
    
    memset(blockaddr, 0, LOOPER_BLOCK_SIZE);

    block = offset / LOOPER_BLOCK_SIZE;
    
    spin_lock_irqsave(&looper_mem_lock, flags);

    next = free_list_end + 1;
    if (next >= LOOPER_MAX_BLOCKS)
	next = 0;
    if (next != free_list_start)
    {
	free_list_end = next;
	free_list[free_list_end] = block;
    }

    spin_unlock_irqrestore(&looper_mem_lock, flags);
}
