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
#include <sys/wait.h>
#include <sys/mount.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include "../modules/spi.h"
#include "../modules/sport.h"
#include "userif.h"
#include "menus.h"
#include "config.h"
#include "midisync.h"

#undef DEBUG_MIDI

pthread_cond_t 	g_display_request 		= PTHREAD_COND_INITIALIZER;
int		g_display_request_flag		= 0;
t_display_func 	g_display_func 			= NULL;
void*		g_display_func_arg 		= NULL;
t_display_func 	g_parent_display_func 		= NULL;
void*		g_parent_display_func_arg 	= NULL;
int lp1sd_mount_result = -1;

void *signal_handler(void *arg);
void *front_panel_handler(void *arg);
void *midi_handler(void *arg);
void *track_status_handler(void *arg);
void *scrambler(void *arg);
void *udp_handler(void *arg);
void *display_thread( void* arg );
void start_network(void);

typedef struct _scancode_entry
{
	unsigned short scancode_queue;	/* Scancode from user i/f */
	unsigned short cc_queue;		/* Continuous controller # */
} scancode_entry;

#define k_scancode_queue_size 1024

scancode_entry	scancode_queue[ k_scancode_queue_size ];
int	scancode_queue_in	= 0;
int scancode_queue_out	= 0;

pthread_mutex_t userif_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t display_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t button_cond = PTHREAD_COND_INITIALIZER;

int spifd;
int audiofd;
int midifd;

FILE *logfp = NULL;

unsigned long midi_clock_handlers[2];
short midi_bypass = 0;

static inline void queue_scancode( int scancode, int cc )
{
	scancode_queue[ scancode_queue_in ].scancode_queue = scancode;
	scancode_queue[ scancode_queue_in ].cc_queue = cc;
	++scancode_queue_in;
	if( scancode_queue_in >= k_scancode_queue_size )
	{
		scancode_queue_in = 0;
	}
}

void queue_udp_command(int func, int arg)
{
    pthread_mutex_lock(&userif_lock);
    queue_scancode( 0x0200 | (func & 0xff), (arg & 0xffff) );
    pthread_cond_signal(&button_cond);
    pthread_mutex_unlock(&userif_lock);
}

int main()
{
    unsigned char buffer[20];
    sigset_t set;
    pthread_t tid;
    short i;

    mknod("/dev/sport1", S_IFCHR | 0600, (251 << 8) | 1);
    mknod("/dev/sport2", S_IFCHR | 0600, (251 << 8) | 2);
    mknod("/dev/sport3", S_IFCHR | 0600, (251 << 8) | 3);
    mknod("/dev/sport4", S_IFCHR | 0600, (251 << 8) | 4);
    mknod("/dev/sport5", S_IFCHR | 0600, (251 << 8) | 5);
    mknod("/dev/sport6", S_IFCHR | 0600, (251 << 8) | 6);
    mknod("/dev/sport7", S_IFCHR | 0600, (251 << 8) | 7);
    mknod("/dev/sport8", S_IFCHR | 0600, (251 << 8) | 8);

    /* Read configuration */
    config_load();
    config_restore_levels(0);

    /* Start network */
    start_network();

    /* Open the front panel device */
    spifd = open("/dev/spi", O_RDWR);
    if (spifd < 0)
    {
	printf("Failed to open spi device.\n");
	return 0;
    }

    /* Open the audio device */
    audiofd = open("/dev/looper", O_RDWR);
    if (audiofd < 0)
    {
	printf("Failed to open audio device.\n");
	return 0;
    }

    ioctl(audiofd, CMD_SPORT_RECORDLEVELTRIGGER, 
	  (unsigned long) looper_config.record_level_trigger);

    /* Open the MIDI device */
    midifd = open("/dev/midi", O_RDWR);
    if (midifd < 0)
    {
	printf("Failed to open MIDI device.\n");
	return 0;
    }

//    logfp = fopen("/var/log/lp1.log", "w");
    logfp = stdout;

    ioctl(audiofd, CMD_GET_MIDI_CLOCK_ADDR, midi_clock_handlers);
    ioctl(midifd, CMD_SPI_SET_MIDI_CLOCK_HANDLER, midi_clock_handlers);

    /* Turn off all of the LEDs */
    for (i = 1; i <= 8; i++)
	ioctl(spifd, CMD_SPI_LED_RED, (unsigned long) i);

    /* Mount audio storage */
    lp1sd_mount_result = mount("/dev/lp1sda1", "/mnt", "vfat", 0, "");

    /* Initialize display */
    display_initialize_display(spifd);

    /* Turn off all of the LEDs */
    for (i = 1; i <= 8; i++)
    {
	ioctl(audiofd, CMD_SPORT_ERASE, (unsigned long) i);
	ioctl(spifd, CMD_SPI_LED_OFF, (unsigned long) i);
    }

    /* Create threads. */
    sigfillset(&set);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
    
    pthread_create(&tid, NULL, signal_handler, NULL);
    pthread_create(&tid, NULL, front_panel_handler, NULL);
    pthread_create(&tid, NULL, midi_handler, NULL);
    pthread_create(&tid, NULL, track_status_handler, (void *) spifd);
    pthread_create(&tid, NULL, scrambler, NULL);
//    pthread_create(&tid, NULL, scrambler, (void *) 1);
    pthread_create(&tid, NULL, udp_handler, NULL);
    pthread_create(&tid, NULL, display_thread, (void*)spifd );

    /* Wait for input event */
    while( 1 )
    {
		menu_run( spifd );
    }

    return( 0 );
}

unsigned long wait_for_input(void)
{
    struct stat statbuf;
    unsigned long scancode;
    pid_t pid;
    int status;
    
    pthread_mutex_lock( &userif_lock );

    while( scancode_queue_in == scancode_queue_out )
	pthread_cond_wait(&button_cond, &userif_lock);
	
    scancode = (unsigned long)
	scancode_queue[ scancode_queue_out ].scancode_queue;
    scancode |= ((unsigned long)
		 scancode_queue[ scancode_queue_out ].cc_queue << 16);

    ++scancode_queue_out;
    if( scancode_queue_out >= k_scancode_queue_size )
    {
	scancode_queue_out = 0;
    }
    
    pthread_mutex_unlock( &userif_lock );

    if (scancode == 0x1234 && stat("/mnt/installer", &statbuf) == 0)
    {
	pthread_mutex_lock( &display_lock );
		
	write(spifd,"\x1f\x24\x00\x00\x00\x00", 6);
	write(spifd, "Upgrading...    ", 16);
	chdir("/mnt");
	
	pid = vfork();
	if (pid == 0)	/* Child */
	{
	    execl("/mnt/installer", "installer", NULL);
	    exit(-1);
	}

	if (pid < 0)
	{
	    printf("pid < 0\n");
	    write(spifd,"\x1f\x24\x00\x00\x00\x00", 6);
	    write(spifd, "Upgrade failed  ", 16);
	}
	else if (waitpid(pid, &status, 0) < 0)
	{
	    printf("waitpid failed\n");
	    write(spifd,"\x1f\x24\x00\x00\x00\x00", 6);
	    write(spifd, "Upgrade failed  ", 16);
	}
	else if (status != 0)
	{
	    printf("status == %d\n", status);
	    write(spifd,"\x1f\x24\x00\x00\x00\x00", 6);
	    write(spifd, "Upgrade failed  ", 16);
	}
	else
	{
	    write(spifd,"\x1f\x24\x00\x00\x00\x00", 6);
	    write(spifd, "Upgrade done    ", 16);
	}

	write(spifd, "\x1f\x24\x00\x00\x01\x00Please reboot", 19);
	sleep(2);
//	system("/bin/reboot");
	while (1)
	    ;
		
	pthread_mutex_unlock( &display_lock );
    }

    return scancode;
}

void *signal_handler(void *arg)
{
    sigset_t set;
    int sig;
    
    sigfillset(&set);
    
    while (1) 
    {
	sigwait(&set, &sig);
	
	switch (sig)
	{
	  case SIGINT:
	    exit(0);

	  case SIGHUP:
	    pthread_mutex_lock(&userif_lock);

	    queue_scancode( 0x1234, 0x0000 );
	  
	    pthread_cond_signal(&button_cond);
	    pthread_mutex_unlock(&userif_lock);
	    break;

	  default:
	    printf("Caught unexpected signal %d\n", sig);
	    break;
	}
    }

    return NULL;
}

void *front_panel_handler(void *arg)
{
    unsigned short scancode;
    
    while( 1 )
    {
	if( looper_config.front_panel_version == 99)
	{
	    sleep(1);
	}
	else if( read( spifd, &scancode, 2) == 2 )
	{
	    if( looper_config.front_panel_version > 0 )
	    {
		if( (scancode & 0x00f0) == 0x10 )
		    scancode += 0x10;
		else if( (scancode & 0x00f0) == 0x20 )
		    scancode -= 0x10;
	    }
			
	    pthread_mutex_lock( &userif_lock );

	    queue_scancode( scancode, 0x0000 );

	    pthread_cond_signal( &button_cond );
	    pthread_mutex_unlock( &userif_lock );
	}
    }
}

#ifndef USE_NEW_MIDI_HANDLER
static unsigned short controller_values[128];
static unsigned char msgbytes[5];

void *midi_handler(void *arg)
{
    static unsigned short last_channel_command = 0;
    static unsigned short last_command = 0;
    static unsigned short last_channel = 0;
    static unsigned short message_position = 0;
    unsigned char c;
    unsigned short add_to_queue;
    unsigned short controller_number;
    
    printf( "[ old midi handler ]\n" );
    for (controller_number = 0; controller_number < 128; controller_number++)
	controller_values[controller_number] = 0xffff;

    while (1)
    {
	if (read(midifd, &c, 1) == 1)
	{
	    controller_number = 0;
		
#ifdef DEBUG_MIDI
	    if (logfp != NULL && c != 0xfe)
	    {
		if (c & 0x80)
		    fprintf(logfp, "\n");
		fprintf(logfp, "%x ", c);
		fflush(logfp);
	    }
#endif /* DEBUG_MIDI */

	    if (midi_bypass)
	    {
		write(midifd, &c, 1);
		continue;
	    }

	    /*
	      System commands
	    */
	    if ((c & 0xf0) == 0xf0)	/* Start of system command */
	    {
		switch (c)
		{
		  case 0xf0:
		  case 0xf2:
		  case 0xf3:
		    last_command = c;
		    break;
		  default:
		    last_command = last_channel_command;
		    break;
		}
		
		message_position = 0;
	    }
	    /*
	      Channel commands
	    */
	    else if ((c & 0x80))
	    {
		last_command = (c & 0xf0);
		last_channel_command = last_command;
		last_channel = (c & 0x0f);
		message_position = 0;
	    }
	    /*
	      Data byte
	    */
	    else if (!(c & 0x80))
	    {
		add_to_queue = 0;
		
		if (message_position < 5)
		{
		    msgbytes[message_position] = c;

//		    printf("mb[%d] = %x\n", message_position, (int) c);
		}
		
		message_position++;
		
		switch (last_command)
		{
		  case 0xf2:		/* song position */
		    if (message_position >= 2)
		    {
			last_command = last_channel_command;
			message_position = 0;
		    }
		    break;

		  case 0xf3:		/* song select */
		    last_command = last_channel_command;
		    message_position = 0;
		    break;

		  case 0x80:		/* note off */
		    if (message_position >= 2)
		    {
			message_position = 0;
			
			if (last_channel == looper_config.midi_channel)
			    add_to_queue = 0xa000 | msgbytes[0];
		    }
		    break;

		  case 0x90:		/* note on */
		    if (message_position >= 2)
		    {
			message_position = 0;

			if (last_channel == looper_config.midi_channel)
			{
			    if (msgbytes[1] == 0)
				add_to_queue = 0xa000 | msgbytes[0];
			    else
				add_to_queue = 0x9000 | msgbytes[0];
			}
		    }
		    break;

		  case 0xb0:		/* control change */
		    if (message_position >= 2)
		    {
			message_position = 0;

			if (last_channel == looper_config.midi_channel)
			{
			    unsigned short *cvp;
			    
			    controller_number = msgbytes[0];
			    controller_number &= 0xff;
			
			    cvp = &controller_values[controller_number];
			    
			    if ((*cvp != 0xffff && *cvp != msgbytes[1]) ||
				looper_config.midi_pgm_change[controller_number + 128][0] >= 0)
			    {
				add_to_queue = 0xb000 | msgbytes[1];
			    }
			
			    controller_values[controller_number] = msgbytes[1];
			}
		    }

		    break;

		  case 0xc0:		/* program change */
		    message_position = 0;

		    if (last_channel == looper_config.midi_channel)
			add_to_queue = 0xc000 | msgbytes[0];
		    break;

		  case 0xa0:		/* poly key pressure */
		  case 0xd0:		/* channel pressure */
		  case 0xe0:		/* pitch wheel change */
		  case 0xf0:		/* sysex */
		  default:
		    /* Do nothing */
		    break;
		}

		if (add_to_queue != 0)
		{
//		    printf("Queuing %x\n", add_to_queue);
		    
		    pthread_mutex_lock(&userif_lock);

			queue_scancode( add_to_queue, controller_number );

		    pthread_cond_signal(&button_cond);
		    pthread_mutex_unlock(&userif_lock);
		}
	    }
	}
    }
}
#else
/* --- cpr --- */
/* for integration purposes only */
/* implement previous behaviour using new midi_handler here */
/* final code will call the real midi_remote_control_callback which will */
/* call any LP1 commands bound to a midi event */
static unsigned short controller_values[128] = 
{
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF
};

void midi_remote_control_callback( int command, int data1, int data2 )
{
	unsigned short add_to_queue = 0;
    unsigned short controller_number = 0;
	
	switch( command & 0xF0 )
	{
		case 0x80 :	/* note off */
		{
			if( (command & 0x0F) == looper_config.midi_channel )
			{
				add_to_queue = 0xa000 | data1;
		    }
		}
		break;
		
	  	case 0x90: /* note on */
		{
			if( (command & 0x0F) == looper_config.midi_channel )
			{
				/* is this really a note off (ie note on with velocity of 0)? */
			    if( data2 == 0 )
				{
					add_to_queue = 0xa000 | data1;
				}
			    else
				{
					add_to_queue = 0x9000 | data1;
				}
			}
	    }
	    break;
		
	  	case 0xb0: /* control change */
		{
			if( (command & 0x0F) == looper_config.midi_channel )
			{
			    controller_number = data1;
			
				/* --- cpr --- */
				/* possible problem? */
				/* CC will not be sent to LP1 the first time it comes in */
				/* because controller_values[ data1 ] will equal 0xFFFF, */
				/* which will cause the add_to_queue setup to be skipped? */
			    if( (controller_values[ controller_number ] != 0xffff) &&
					(controller_values[ controller_number ] != data2)    )
			    {
					add_to_queue = 0xb000 | data2;
			    }
			
			    controller_values[ controller_number ] = data2;
			}
	    }
	    break;
		
		case 0xc0: /* program change */
		{
			if( (command & 0x0F) == looper_config.midi_channel )
			{
				add_to_queue = 0xc000 | data1;
			}
		}
		break;
	}

	if( add_to_queue != 0 )
	{
//		    printf("Queuing %x\n", add_to_queue);
		
		pthread_mutex_lock( &userif_lock );

		queue_scancode( add_to_queue, controller_number );

		pthread_cond_signal( &button_cond );
		pthread_mutex_unlock( &userif_lock );
	}
}
#endif /* USE_NEW_MIDI_HANDLER */

void* dhcpcd_handler( void* arg )
{
    while (1)
    {
	system("dhcpcd -D -I looper -h looper");
	sleep(10);
    }
}

void start_network(void)
{
    char cmd[128];
//    pthread_t tid;
    
    /* Automatic if any of the ip addresses not specified */
    if (looper_config.ipaddr == 0 || looper_config.gwaddr == 0 ||
	looper_config.netmask == 0 || looper_config.dnsaddr == 0)
    {
	system("ifconfig eth0 192.168.1.99 up");
//	pthread_create(&tid, NULL, dhcpcd_handler, NULL);
	system("dhcpcd -D -I looper -h looper &");
    }
    else
    {
	FILE *fp;
	
	fp = fopen("/etc/resolv.conf", "w");
	fprintf(fp, "nameserver %d.%d.%d.%d\n",
		(int) ((looper_config.dnsaddr >> 24) & 0xff),
		(int) ((looper_config.dnsaddr >> 16) & 0xff),
		(int) ((looper_config.dnsaddr >> 8) & 0xff),
		(int) (looper_config.dnsaddr & 0xff));
	fprintf(fp, "search looperlative.com\n");
	fclose(fp);
	
	sprintf(cmd, "ifconfig eth0 %d.%d.%d.%d netmask %d.%d.%d.%d up",
		(int) ((looper_config.ipaddr >> 24) & 0xff),
		(int) ((looper_config.ipaddr >> 16) & 0xff),
		(int) ((looper_config.ipaddr >> 8) & 0xff),
		(int) (looper_config.ipaddr & 0xff),
		(int) ((looper_config.netmask >> 24) & 0xff),
		(int) ((looper_config.netmask >> 16) & 0xff),
		(int) ((looper_config.netmask >> 8) & 0xff),
		(int) (looper_config.netmask & 0xff));
	system(cmd);

	sprintf(cmd, "route add default gw %d.%d.%d.%d",
		(int) ((looper_config.gwaddr >> 24) & 0xff),
		(int) ((looper_config.gwaddr >> 16) & 0xff),
		(int) ((looper_config.gwaddr >> 8) & 0xff),
		(int) (looper_config.gwaddr & 0xff));
	system(cmd);
    }

    system("boa -c /j &");
    system("/j/upgrade_server -d &");
}

void midi_key_startstop(struct menu_s *parent, int displayfd, int arg)
{
    static short midi_started = 0;
    unsigned char cmd;

    if (midi_started)
	cmd = 0xfc;
    else
	cmd = 0xfa;

    write(midifd, &cmd, 1);
    midi_started ^= 1;
}

void midi_key_stop(struct menu_s *parent, int displayfd, int arg)
{
    unsigned char cmd;

    cmd = 0xfc;

    write(midifd, &cmd, 1);
}

static char scrambler_block[125000];

void *scrambler(void *arg)
{
    nice(20);

    while (1)
    {
	if (arg != NULL)
	{
#if 0
	    int fd;
		
	    fd = open("mtdblock3", O_RDONLY);

	    while (read(fd, scrambler_block, sizeof(scrambler_block)) == 
		   sizeof(scrambler_block))
		;
	
	    close(fd);
#endif
	    system("cat /dev/mtdblock3 > /dev/null");
	}
	else
	    read(audiofd, scrambler_block, sizeof(scrambler_block));
    }
}

void midi_key_bypass(struct menu_s *parent, int displayfd, int arg)
{
    midi_bypass = !midi_bypass;
}

void display_update( void )
{
	pthread_mutex_lock( &display_lock );
	
	/* check if last display request hasn't been processed yet */
	if( !g_display_request_flag )
	{
		/* set display request flag */
		g_display_request_flag = 1;
		
		/* wake up display thread by signaling display request condition */
		pthread_cond_signal( &g_display_request );
	}

    pthread_mutex_unlock( &display_lock );
}


void* display_thread( void* arg )
{
	int displayfd = (int)arg;

    while( 1 )
    {
        pthread_mutex_lock( &display_lock );

		/* wait for display request to be signalled */
        while( !g_display_request_flag )
            pthread_cond_wait( &g_display_request, &display_lock );

		/* call display function */
		(g_display_func)( g_display_func_arg, displayfd );

		g_display_request_flag = 0;

        pthread_mutex_unlock( &display_lock );
    }
}
