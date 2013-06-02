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

#ifndef __SPI_H__
#define __SPI_H__

struct spi_s
{
    int comm_mode;
    
    /* 8-bit device queue.  Devices on slave select PF2 */
    unsigned short spi_send_q[128];
    short spi_send_q_in;
    short spi_send_q_out;

    /* MIDI input queue */
    unsigned char midi_in_q[256];
    short midi_in_q_in;
    short midi_in_q_out;

    /* MIDI output queue */
    unsigned char midi_out_q[256];
    short midi_out_q_in;
    short midi_out_q_out;

    /* LED images.  Supports blinking LEDs */
    unsigned short led_image[2];
    unsigned short current_image;

    /* Front panel button data structures */
    unsigned short key_scan_delay;
    unsigned short key_column;
    unsigned short key_state;
    unsigned short key_scancode;

    /* Rotary encoder data */
    unsigned short encoder_state;
    unsigned short encoder_lastval;
    unsigned short encoder_debounce;
    int encoder_change;

    /* Display interface state */
    unsigned short display_busy;

    /* Miscellaneous */
    unsigned long interrupts;
};

struct track_timer_s
{
    short x;
    short y;
    short value;
};

struct volume_bar_s
{
    unsigned short in_mask;
    unsigned short out_mask;
};

#define SPI_COMM_MODE_NONE	0
#define SPI_COMM_MODE_PF2	1
#define SPI_COMM_MODE_MIDI_RX	2
#define SPI_COMM_MODE_MIDI_TX	3

#define SPI_KEY_UP		0
#define SPI_KEY_DEBOUNCEDOWN	1
#define SPI_KEY_DEBOUNCEUP	2
#define SPI_KEY_DOWN		3

#define SPI_KEY_DEBOUNCEDELAY	50
#define SPI_KEY_SCANDELAY	10

#define CMD_SPI_LED_OFF		1
#define CMD_SPI_LED_GREEN	2
#define CMD_SPI_LED_RED		3
#define CMD_SPI_LED_GREENFLASH	4
#define CMD_SPI_LED_REDFLASH	5
#define CMD_SPI_LED_REDGREENFLASH	6

#define CMD_SPI_DISPLAY_TRACK_TIMER	7
#define CMD_SPI_DISPLAY_VOLUME_BARS	8

#define CMD_SPI_SET_MIDI_CLOCK_HANDLER 6

#endif /* __SPI_H__ */
