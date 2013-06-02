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
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include "../modules/spi.h"
#include "../modules/sport.h"
#include "userif.h"
#include "menus.h"
#include "config.h"

pthread_mutex_t midisync_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned short midi_periods[4] = { 0, 0, 0, 0 };
static short next_idx = 0;
static unsigned long total_time = 0;

void midi_report_period(unsigned long midi_period)
{
    pthread_mutex_lock(&midisync_lock);

    if (midi_period > 30000)
	midi_period = 30000;
    
    total_time -= midi_periods[next_idx];
    total_time += midi_period;

    midi_periods[next_idx++] = (unsigned short) midi_period;
    next_idx &= 0x3;

    pthread_mutex_unlock(&midisync_lock);
}

unsigned short midi_get_bpm(void)
{
    unsigned short average;
    unsigned short i;
    
    average = (unsigned short) (total_time >> 2);

    pthread_mutex_lock(&midisync_lock);

    for (i = 0; i < 4; i++)
    {
	if (average > midi_periods[i] && average - 500 > midi_periods[i])
	{
	    pthread_mutex_unlock(&midisync_lock);
	    return 0;
	}
	else if (average < midi_periods[i] && average + 500 < midi_periods[i])
	{
	    pthread_mutex_unlock(&midisync_lock);
	    return 0;
	}
    }

    pthread_mutex_unlock(&midisync_lock);

    if (average == 0)
	return 0;

    return (unsigned short) 
	(((1323000L / (SPORT_DMA_LONGS / 4)) + (average / 2)) / average);
}
