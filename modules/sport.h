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

#ifndef __SPORT_H__
#define __SPORT_H__

#define SPORT_DMA_BUFS		20
//#define SPORT_DMA_BUFS		10	/* was 5 */

#define SPORT_SG_LONGS		2

/* DMA_LONGS was 16 */
#define SPORT_DMA_LONGS		4
//#define SPORT_DMA_LONGS		16

#define SPORT_SG_BYTES		(SPORT_SG_LONGS * 4)
#define SPORT_DMA_BYTES		(SPORT_DMA_LONGS * 4)

#define CMD_SPORT_LOOPSTATUS	1
#define CMD_SPORT_SETMASTER	2
#define CMD_SPORT_STOP		3
#define CMD_SPORT_PLAY		4
#define CMD_SPORT_RECORD	5
#define CMD_SPORT_OVERDUB	6
#define CMD_SPORT_ERASE		7
#define CMD_SPORT_REVERSE	8
#define CMD_SPORT_DOUBLESPEED	11
#define CMD_SPORT_HALFSPEED	12
#define CMD_SPORT_TRACKLEVEL	13
#define CMD_SPORT_PLAYNOW	14
#define CMD_SPORT_STOPNOW	15
#define CMD_SPORT_NOISEGATELVL	16
#define CMD_SPORT_NOISEGATETIME	17
#define CMD_SPORT_TRACKFEEDBACK	18
#define CMD_SPORT_MASTERVOLUME	19
#define CMD_SPORT_MULTIPLY	20
#define CMD_MIDI_CLOCK		21
#define CMD_MIDISYNC_RECORD	22
#define CMD_SPORT_WETONLY	23
#define CMD_SPORT_CALIBRATE	24
#define CMD_MIDISYNC_BPMEASURE	25
#define CMD_SPORT_CUE		26
#define CMD_SYNC_RECORD		27
#define CMD_GET_MIDI_CLOCK_ADDR	28
#define CMD_SPORT_REPLACE	29
#define CMD_SPORT_ASSIGNAUX1	30
#define CMD_SPORT_SCRAMBLE	31
#define CMD_SPORT_SETSPEED	32
#define CMD_SPORT_MIDISYNCOUTENABLE	33
#define CMD_SPORT_ASSIGNAUX2 	34
#define CMD_SPORT_BOUNCE 	35
#define CMD_SPORT_VOLUMESPEED	36
#define CMD_SPORT_PAN		37
#define CMD_SPORT_QREPLACE	38
#define CMD_SPORT_SETQUANT	39
#define CMD_SPORT_COPY		40
#define CMD_SPORT_UNDO		41
#define CMD_SPORT_RECORDLEVELTRIGGER	42

#define CMD_SPORT_OPTION_TRACKNUM	0x0000000f
#define CMD_SPORT_OPTION_FILL		0x00010000

#define SPORT_ALL_TRACKS	0

#define SPORT_TRACK_STATUS_EMPTY	0
#define SPORT_TRACK_STATUS_PLAYING	1
#define SPORT_TRACK_STATUS_STOPPED	2
#define SPORT_TRACK_STATUS_RECORDING	3
#define SPORT_TRACK_STATUS_OVERDUBBING	4
#define SPORT_TRACK_STATUS_REPLACING	5
#define SPORT_TRACK_STATUS_BOUNCING	6
#define SPORT_TRACK_STATUS_ARMED	7

struct sport_track_level_s
{
    short track;
    short level;
};

struct sport_loop_status_s
{
    unsigned long track_states;
    short master_track;
    long max_pos_value;
    long min_pos_value;
    long max_out_value;
    long noise_gate_level;
    long track_position[8];
    long track_length[8];
    long midi_period;
    short track_speeds[8];
    long midi_error[8];
};

#define SPORT_SYNC_NONE		0
#define SPORT_SYNC_INTERNAL	1
#define SPORT_SYNC_MIDI		2

struct sport_bounce_s
{
    unsigned char source_mask;
    short destination_track;
    int sync_type;
};

struct sport_track_file_s
{
    short track;
    long pos;
    long read_count;
};

#endif /* __SPORT_H__ */
