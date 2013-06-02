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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <string.h>
#include "tcp.h"
#include "../modules/sport.h"

int debug_level = 0;
int local_port = 8080;

unsigned char readbuf[64 * 1024];
unsigned char linebuf[1024];
unsigned char separator[256] = "--";
unsigned char *readbuf_p = NULL;
int readbuf_pos = 0;
int readbuf_limit = 0;

int upgrade_fd;

unsigned char *mygetline(int fd, int *n_read)
{
    int n;
    unsigned char c;
    unsigned char *p;

    p = linebuf;
    for (n = 0; n < sizeof(linebuf) - 1; )
    {
	if (readbuf_p == NULL || readbuf_pos >= readbuf_limit)
	{
	    readbuf_limit = read(fd, readbuf, sizeof(readbuf));
	    if (readbuf_limit <= 0)
		break;

	    readbuf_p = readbuf;
	    readbuf_pos = 0;
	}
	
	c = *readbuf_p;
	*p++ = *readbuf_p++;
	readbuf_pos++;
	n++;
	if (c == '\n')
	    break;
    }

    *p = '\0';
    *n_read = n;
    return linebuf;
}

int parse_upgrade(int fd)
{
    char *s;
    char *p;
    int state;
    int n_read;
    int upgrade_data = 0;

    /*
      Parse remaining header lines
    */
    while ((s = mygetline(fd, &n_read)))
    {
	printf("%s", s);
	
	if (strncasecmp(s, "Content-Type: ", 14) == 0)
	{
	    p = strstr(s, "boundary=");
	    if (p == NULL)
		return -1;
	    strncpy(separator + 2, p + 9, sizeof(separator));
	    p = strchr(separator, ';');
	    if (p == NULL)
		p = strchr(separator, '\r');
	    if (p == NULL)
		p = strchr(separator, '\n');
	    if (p != NULL)
		*p = '\0';
	}

	else if (strcmp(s, "\r\n") == 0)
	    break;
    }
    
    upgrade_fd = open("/tmp/lp1upgrade.tar", 
		      O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (upgrade_fd < 0)
	return -1;

    state = 0;
    while (1)
    {
	s = mygetline(fd, &n_read);

	if (n_read == 0)
	{
	    close(upgrade_fd);
	    return -1;
	}

	if (strncmp(s, separator, strlen(separator)) == 0)
	{
	    if (s[strlen(separator)] == '-')
		break;
	    
	    state = 0;
	}
    
	switch (state)
	{
	  case 0:		/* Looking for content headers */
	    if (strncmp(s, "Content-Disposition:", 20) == 0)
	    {
		if (strstr(s, "name=\"upgrade\"") != NULL)
		{
		    upgrade_data = 1;
		}
		else
		{
		    upgrade_data = 0;
		}
	    }
	    if (strcmp(s, "\r\n") == 0)
	    {
		state = 1;
	    }
	    break;

	  case 1:		/* Storing data */
	    if (upgrade_data)
	    {
		write(upgrade_fd, s, n_read);
	    }
	    break;
	}
    }

    close(upgrade_fd);
    return 0;
}

short riff_state = 0;
short riff_error = 0;

unsigned char riff_buffer[4];
short riff_buffer_idx = 0;
short riff_buffer_expect = 4;

unsigned long *riff_lp = (unsigned long *) riff_buffer;
unsigned short *riff_sp = (unsigned short *) riff_buffer;

unsigned long riff_chunk_size = 0;
unsigned long riff_chunk_idx = 0;

void parse_riff(unsigned char c, int audio_fd)
{
    if (riff_error)
	return;
    
    if (riff_buffer_idx < 4)
	riff_buffer[riff_buffer_idx++] = c;

    if (riff_chunk_size > 0 && ++riff_chunk_idx > riff_chunk_size)
    {
	riff_buffer_expect = 4;
	riff_state = 3;
	riff_chunk_size = 0;
    }
    
    if (riff_buffer_idx == riff_buffer_expect)
    {
	switch (riff_state)
	{
	  case 0:	/* RIFF id */
	    if (strncmp(riff_buffer, "RIFF", 4) != 0)
	    {
		printf("RIFF id not found\n");
		riff_error = 1;
	    }
	    riff_state++;
	    break;
	    
	  case 1:	/* FILE size */
	    riff_state++;
	    break;

	  case 2:	/* Format */
	    if (strncmp(riff_buffer, "WAVE", 4) != 0)
	    {
		printf("WAVE id not found\n");
		riff_error = 1;
	    }
	    riff_state++;
	    break;

	  case 3:	/* Subchunk ID */
	    if (strncmp(riff_buffer, "fmt ", 4) == 0)
		riff_state = 100;
	    else if (strncmp(riff_buffer, "data", 4) == 0)
		riff_state = 200;
	    else
		riff_state = 300;
	    break;
	    
	  case 100:	/* fmt chunk size */
	    riff_chunk_size = *riff_lp;
	    riff_buffer_expect = 2;
	    riff_chunk_idx = 0;
	    riff_state++;
	    break;
	    
	  case 200:	/* data chunk size */
	    riff_chunk_size = *riff_lp;
	    riff_buffer_expect = 3;
	    riff_chunk_idx = 0;
	    riff_state++;
	    break;

	  case 300:	/* unknown chunk size */
	    riff_chunk_size = *riff_lp;
	    riff_buffer_expect = 4;
	    riff_chunk_idx = 0;
	    riff_state++;
	    break;

	  case 101:	/* Audio format */
	    if (*riff_sp != 1)
	    {
		printf("Not PCM\n");
		riff_error = 1;
	    }
	    riff_state++;
	    break;

	  case 102:	/* Audio channels */
	    if (*riff_sp != 2)
	    {
		printf("Not Stereo\n");
		riff_error = 1;
	    }
	    riff_buffer_expect = 4;
	    riff_state++;
	    break;

	  case 103:	/* Sample rate */
	    if (*riff_lp != 44100)
	    {
		printf("Not 44100\n");
		riff_error = 1;
	    }
	    riff_state++;
	    break;

	  case 104:	/* Byte rate */
	    riff_buffer_expect = 2;
	    riff_state++;
	    break;

	  case 105:	/* Block align */
	    if (*riff_sp != 6)
		riff_error = 1;
	    riff_state++;
	    break;

	  case 106:	/* Bits per sample */
	    if (*riff_sp != 24)
		riff_error = 1;
	    riff_state++;
	    break;

	  case 201:	/* Audio data */
	    if (riff_buffer[2] & 0x80)
		riff_buffer[3] = '\xff';
	    else
		riff_buffer[3] = '\x00';
	    write(audio_fd, riff_buffer, 4);
	    break;
	}

	riff_buffer_idx = 0;
    }
}

int upload_audio(int fd, int track)
{
    struct sport_loop_status_s ls;
    char *s;
    char *p;
    int state;
    int n_read;
    int audio_data = 0;
    int audio_fd;
    int i;

    if (track < 1 || track > 8)
	return -1;

    riff_state = 0;
    riff_error = 0;
    riff_buffer_idx = 0;
    riff_buffer_expect = 4;
    riff_chunk_size = 0;
    riff_chunk_idx = 0;

    /*
      Parse remaining header lines
    */
    while ((s = mygetline(fd, &n_read)))
    {
	printf("%s", s);
	
	if (strncasecmp(s, "Content-Type: ", 14) == 0)
	{
	    p = strstr(s, "boundary=");
	    if (p == NULL)
		return -1;
	    strncpy(separator + 2, p + 9, sizeof(separator));
	    p = strchr(separator, ';');
	    if (p == NULL)
		p = strchr(separator, '\r');
	    if (p == NULL)
		p = strchr(separator, '\n');
	    if (p != NULL)
		*p = '\0';
	}

	else if (strcmp(s, "\r\n") == 0)
	    break;
    }

    sprintf(linebuf, "/dev/sport%d", track);
    audio_fd = open(linebuf, O_RDWR, 0777);
    if (audio_fd < 0)
	return -1;

    ioctl(audio_fd, CMD_SPORT_LOOPSTATUS, &ls);
    if (ls.track_length[track-1] != 0)
    {
	close(audio_fd);
	return -1;
    }

    state = 0;
    while (1)
    {
	s = mygetline(fd, &n_read);

	if (n_read == 0)
	{
	    close(audio_fd);
	    return -1;
	}

	if (strncmp(s, separator, strlen(separator)) == 0)
	{
	    if (s[strlen(separator)] == '-')
		break;
	    
	    state = 0;
	}
    
	switch (state)
	{
	  case 0:		/* Looking for content headers */
	    if (strncmp(s, "Content-Disposition:", 20) == 0)
	    {
		if (strstr(s, "name=\"upload\"") != NULL)
		{
		    printf("Found audio data\n");
		    audio_data = 1;
		}
		else
		{
		    audio_data = 0;
		}
	    }
	    if (strcmp(s, "\r\n") == 0)
	    {
		state = 1;
	    }
	    break;

	  case 1:		/* Storing data */
	    if (audio_data)
	    {
		/* Parse WAV file */
		for (i = 0; i < n_read; i++)
		    parse_riff(s[i], audio_fd);
	    }
	    break;
	}
    }

    close(audio_fd);
    return 0;
}

void handle_http_connection(int remotefd)
{
    char *s;
    int n_read;
    FILE *fp;
    int error;
    char *message = "";
    
    /*
      Determine what page the user is requesting.
    */
    s = mygetline(remotefd, &n_read);

    /*
      Upgrade page?
    */
    if (strncasecmp(s, "POST /doupgrade HTTP", 20) == 0)
    {
	error = parse_upgrade(remotefd);

	if (!error)
	{
	    chdir("/mnt");
	    system("tar -x < /tmp/lp1upgrade.tar > /dev/null 2>&1");
	    system("killall -1 userif > /dev/null 2>&1");

	    message = "Upgrade installing.  Please watch the front panel of"
		" LP1 for further information.<p>";
	}
	else
	    message = "Upgrade failed to upload.<p>";
    }

    /*
      Upload audio?
    */
    else if (strncasecmp(s, "POST /uploadaudio", 17) == 0)
    {
	error = upload_audio(remotefd, atoi(s + 17));

	if (riff_error)
	    message = "Audio file format not recognized.";
	else if (error)
	    message = "Audio upload failed.";
	else
	    message = "Audio upload successful.";
    }

    /*
      Dunno
    */
    else
	message = "Sorry, but I don't know how to process that request.";

    fp = fdopen(remotefd, "w+");
    fprintf(fp, "HTTP/1.0 200 OK\n");
    fprintf(fp, "Content-Type: text/html\n\n");

    fprintf(fp, "<html><body>%s\n", message);
    fprintf(fp, "</body></html>\n");

    fflush(fp);
    fclose(fp);

    printf("Response sent\n");
    
    sleep(5);
    close(remotefd);
}

void childhandler(int signum)
{
    int status;

    while (waitpid(-1, &status, WNOHANG) > 0)
	;
}

int main(int argc, char **argv)
{
    char opt;
    int listenfd;
    int childfd;
    int status;
    char portstr[10];

    while ((opt = getopt(argc, argv, "d")) != EOF)
    {
	if (opt == 'd')
	    debug_level++;
    }
    
    signal(SIGCHLD, childhandler);

    sprintf(portstr, "%d", local_port);
    listenfd = tcp_initsock(portstr);
    while (1)
    {
    	while (waitpid(-1, &status, WNOHANG) > 0)
	    ;

	childfd = tcp_waitforconnect(listenfd);

	handle_http_connection(childfd);
    }	
}
