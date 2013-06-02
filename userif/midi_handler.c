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

#ifdef USE_NEW_MIDI_HANDLER
/******************************************************************************/
/*                                                                            */
/* midi_handler.c                                                             */
/*                                                                            */
/******************************************************************************/

/* includes */
#include <stdio.h>
#include <unistd.h>

/* external references */
extern int midifd;
extern short midi_bypass;
void midi_remote_control_callback( int command, int data1, int data2 );

/* constants */
#define k_status_byte_mask              0x80

#define k_no_midi_command               0x00

/* -------- standard midi messages -------- */
/* --- channel catagory --- */
#define k_note_off_command              0x80
#define k_note_on_command               0x90
#define k_aftertouch_command            0xA0
#define k_control_change_command        0xB0
#define k_program_change_command        0xC0
#define k_channel_pressure_command      0xD0
#define k_pitch_wheel_command           0xE0

/* --- system common --- */
#define k_start_of_sysex_command        0xF0
#define k_mtc_quarter_frame_command     0xF1
#define k_song_position_pointer_command 0xF2
#define k_song_select_command           0xF3
#define k_F4_undefined_command			0xF4
#define k_F5_undefined_command			0xF5
#define k_tune_request_command          0xF6
#define k_end_of_sysex_command          0xF7

/* --- system realtime --- */
#define k_midi_clock_command            0xF8
#define k_midi_tick_command             0xF9
#define k_midi_start_command            0xFA
#define k_midi_continue_command         0xFB
#define k_midi_stop_command             0xFC
#define k_FD_undefined_command			0xFD
#define k_active_sense_command          0xFE
#define k_reset_command                 0xFF

/* mtc */

/* mmc */

unsigned short midi_msg_len_table[] =
{
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0x80 - 0x8F */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0x90 - 0x9F */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0xA0 - 0xAF */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0xB0 - 0xBF */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xC0 - 0xCF */
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, /* 0xD0 - 0xDF */
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, /* 0xE0 - 0xEF */
	0, 2, 3, 2, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1  /* 0xF0 - 0xFF */
};

void *midi_handler( void *arg )
{
    /* static variables */
    static unsigned short last_command          	= k_no_midi_command;
    static unsigned short midi_msg_len          	= 0;
    static unsigned char  midi_message_status_byte	= 0;
	static unsigned char  midi_message_data1_byte	= 0;
	static unsigned char  midi_message_data2_byte	= 0;

    /* local variables */
    unsigned char incoming_midi_byte;
	
	printf( "[ new midi handler }\n" );
	
    while( 1 )
    {
        if( read(midifd, &incoming_midi_byte, 1) == 1 )
        {
#ifdef DEBUG_MIDI
            if( logfp != NULL && incoming_midi_byte != 0xfe )
            {
                if( incoming_midi_byte & 0x80 )
                    fprintf( logfp, "\n" );
                fprintf( logfp, "%x ", incoming_midi_byte );
                fflush( logfp );
            }
#endif /* DEBUG_MIDI */

            if( midi_bypass )
            {
                write( midifd, &incoming_midi_byte, 1 );
                continue;
            }

			/* if this is a status byte */
			if( incoming_midi_byte & k_status_byte_mask )
			{
				/* is this a realtime msgs */
				if( incoming_midi_byte >= k_midi_clock_command )
				{
					/* call the midi callback handler with realtime messsage */
					midi_remote_control_callback( incoming_midi_byte, 0, 0 );							
					
					/* loop back to read next midi byte */
					continue;
				}
				else
				{	/* if midi status byte is an end of sysex command */
					if( incoming_midi_byte == k_end_of_sysex_command )
					{
						/* if we are currently processing a sysex command */
						if( midi_message_status_byte == k_start_of_sysex_command )
						{
							/* place holder for when we do handle sysex */
							/* code should be called that handles the message */
						}
						
						/* reset input state, both for processing the sysex, and */
  						/* if we received an unexpected end of sysex message */
						
						/* reset message length to 0 */				
						midi_msg_len = 0;
						
						/* reset midi message memory */
						midi_message_status_byte = 0;
						midi_message_data1_byte	 = 0;
						midi_message_data2_byte	 = 0;
						
						/* loop back to read next midi byte */
						continue;
					}
					else /* status byte other than realtime or F7 */
					{
						switch( midi_msg_len )
						{
							default : /* 3 bytes or more */ 
							{
								/* this will only happen if we recieve a */
								/* non-F7 status byte while receiving a sysex */
								/* message, in which case we abort the sysex */
								
								/* reset data byte 2 */
								midi_message_data2_byte	 = 0x00;
								
								/* fall thru on purpose */
							}
							case 2 : /* expected data byte 2 */
							{
								/* reset data byte 1 */
								midi_message_data1_byte = 0x00;
								
								/* fall thru on purpose */
							}
							case 0 : /* expecting status byte */
							{
								/* set message length to 1 */
								midi_msg_len = 1;
								
								/* fall thru on purpose */
							}
							case 1 : /* expected data byte 1 */
							{
								/* store status byte into first byte of midi message */
								midi_message_status_byte = incoming_midi_byte;
								
								/* check if this command is a channel command*/
								if( incoming_midi_byte < k_start_of_sysex_command )
								{
									/* channel commands are eligible for running status */
									last_command = incoming_midi_byte;
								}
								else
								{
									/* system messages cancel running status */
									last_command = k_no_midi_command;
								}
							}
							break;
						} /* switch( midi_msg_len ) */
					} /* if( incoming_midi_byte == k_end_of_sysex_command ) */
				} /* if( incoming_midi_byte >= k_midi_clock_command ) */
			} /* if( incoming_midi_byte & k_status_byte_mask ) */
			else /* data byte */
			{
				switch( midi_msg_len )
				{
					case 1 :
					{
						/* set data byte 1 using byte just received */
						midi_message_data1_byte = incoming_midi_byte;
						
						/* set msg length to 2 */
						midi_msg_len = 2;
					}
					break;
					
					case 2 :
					{
						/* set data byte 2 using byte just received */
						midi_message_data2_byte = incoming_midi_byte;

						/* set message length to 3 */
						midi_msg_len = 3;
					}
					break;
					
					case 0 :
					{
						/* is there a previous command */
						/* ie. can we do running status */
						if( last_command != k_no_midi_command )
						{
							/* set command using previous command */
							midi_message_status_byte = last_command;
							
							/* set data byte 1 using byte just received */
							midi_message_data1_byte = incoming_midi_byte;
							
							/* set msg length to 2 */
							midi_msg_len = 2;
						}
						else /* ignore data byte */
						{
							/* jump to read next midi byte */
							continue;
						}
					}
					break;
					
					default : /* 3 bytes or more, will only be here in the case of a sysex */
					{
						/* place holder for when we do handle sysex */
						/* if we decide to handle sysex we need to store */
						/* the data somewhere */
						
						++midi_msg_len;

						/* loop back to read next midi byte */
						continue;
					}
					break;
				} /* end of switch( midi_msg_len ) */
			} /* if( incoming_midi_byte & k_status_byte_mask ) */
				
			/* if there is a message and it's a complete message */
			if( midi_msg_len == midi_msg_len_table[ midi_message_status_byte - k_note_off_command ] )
			{
				/* call the midi callback handler with new messsage */
				midi_remote_control_callback( midi_message_status_byte, midi_message_data1_byte, midi_message_data2_byte );

				/* reset message length to 0 */				
				midi_msg_len = 0;
				
				/* reset midi message memory */
				midi_message_status_byte = 0;
				midi_message_data1_byte	 = 0;
				midi_message_data2_byte	 = 0;
			}
		} /* if( read(midifd, &incoming_midi_byte, 1) == 1 ) */
	} /* while( 1 ) */
}
#endif /* USE_NEW_MIDI_HANDLER */
