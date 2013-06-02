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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include "../modules/sport.h"

unsigned char readbuf[64 * 1024];
unsigned char linebuf[1024];
unsigned char separator[256] = "--";
unsigned char *readbuf_p = NULL;
int readbuf_pos = 0;

int upgrade_fd;

void generate_all_tracks_page(void)
{
    struct sport_loop_status_s ls;
    int sportfd;
    int i;

    printf("Content-type: text/html\n\n");
    printf("<html><head><title>LP1 audio tracks</title></head><body>\n");
    fflush(stdout);

    sportfd = open("/dev/sport1", O_RDWR);
    if (sportfd >= 0)
    {
	ioctl(sportfd, CMD_SPORT_LOOPSTATUS, &ls);

	printf("<table border=1 cellpadding=5>\n");

	for (i = 0; i < 8; i++)
	{
	    printf("<tr>");
	    printf("<td>Track %d</td><td>%d.%02d seconds long</td>\n", 
		   i + 1, 
		   ls.track_length[i] / (8 * 44100),
		   (ls.track_length[i] / (8 * 441)) % 100);
	    printf("<td><a href=\"track%d.wav?download=%d\">EXPORT track audio to your computer</a></td>",
		   i + 1, i + 1);
	    printf("<td><a href=\"track%d.wav?upload=%d\">IMPORT track audio from your computer</a></td>",
		   i + 1, i + 1);
	    printf("</tr>\n");
	    fflush(stdout);
	}

	printf("</table>\n");
    }
//    printf("<p>%s", getenv("QUERY_STRING"));

    printf("</body></html>\n");
}

void download_track_audio(int track)
{
    struct sport_loop_status_s ls;
    char devname[16];
    int sportfd;
    unsigned long ul;
    unsigned short us;
    int nread;
    int i1, i2;

    sprintf(devname, "/dev/sport%d", track);
    sportfd = open(devname, O_RDWR);
    if (sportfd < 0)
	generate_all_tracks_page();
    else
    {
	ioctl(sportfd, CMD_SPORT_LOOPSTATUS, &ls);

	printf("Content-type: audio/x-wav\n\n");
	printf("RIFF");
	
	ul = 36 + (ls.track_length[track-1] / 4) * 3;
	fwrite(&ul, sizeof(ul), 1, stdout);
	
	printf("WAVE");
	printf("fmt ");
	ul = 16;
	fwrite(&ul, sizeof(ul), 1, stdout);

	us = 1;
	fwrite(&us, sizeof(us), 1, stdout);
	us = 2;
	fwrite(&us, sizeof(us), 1, stdout);
	ul = 44100;
	fwrite(&ul, sizeof(ul), 1, stdout);
	ul = 288000;
	fwrite(&ul, sizeof(ul), 1, stdout);
	us = 6;
	fwrite(&us, sizeof(us), 1, stdout);
	us = 24;
	fwrite(&us, sizeof(us), 1, stdout);

	printf("data");
	ul = (ls.track_length[track-1] / 4) * 3;
	fwrite(&ul, sizeof(ul), 1, stdout);
	
	do
	{
	    /* Read a data chunk */
	    nread = read(sportfd, linebuf, sizeof(linebuf));

	    if (nread > 0)
	    {
		/* Pack data to 24-bits */
		i2 = 0;
		for (i1 = 0; i1 < nread; i1 += 4)
		{
		    memcpy(readbuf + i2, linebuf + i1, 3);
		    i2 += 3;
		}

		/* Write data */
		fwrite(readbuf, i2, 1, stdout);
	    }
	}
	while (nread > 0);

	fflush(stdout);
    }
}

void prepare_upload_audio(int track)
{
    char *s;
    
    s = getenv("SERVER_ADDR");

    printf("Content-type: text/html\n\n");
    printf("<html><head><title>LP1 select audio file</title></head><body>\n");

    printf("<form method=post action=\"http://%s:8080/uploadaudio%d\" "
	   "enctype=\"multipart/form-data\">", s, track);

    printf("Select the audio file that you wish to upload to track %d.  ",
	   track);
    printf("This file must be a 24-bit stereo PCM WAV file sampled at ");
    printf("44100 Hz.<p>");

    printf("<input type=\"hidden\" name=\"track\" value=\"%d\">", track);
    printf("<input type=\"file\" name=\"upload\"><p>");
    printf("<input type=\"submit\" name=\"send\" value=\"send\">");
    
    printf("</form></body></html>\n");
}

int main(int argc, char **argv)
{
    int i;

    if (sscanf(getenv("QUERY_STRING"), "download=%d", &i) == 1 &&
	i >= 1 && i <= 8)
    {
	download_track_audio(i);
    }
    if (sscanf(getenv("QUERY_STRING"), "upload=%d", &i) == 1 &&
	i >= 1 && i <= 8)
    {
	prepare_upload_audio(i);
    }
    else
	generate_all_tracks_page();

    return 0;
}
