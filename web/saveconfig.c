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

unsigned char linebuf[1024];

void download_config(void)
{
    int configfd;
    int nread;

    configfd = open("/j/looper.config", O_RDONLY);
    if (configfd < 0)
    {
	printf("Content-type: text/html\n\n");
	printf("<html><head><title>LP1 download error</title></head><body>\n");
	printf("<H2>Couldn't find configuration.</H2>\n");
	printf("</body></html>\n");
	fflush(stdout);
    }
    else
    {
	printf("Content-type: application/vnd.looperlative-config\n\n");
	
	do
	{
	    nread = read(configfd, linebuf, sizeof(linebuf));

	    if (nread > 0)
		fwrite(linebuf, nread, 1, stdout);
	}
	while (nread > 0);

	fflush(stdout);
    }
}

void upload_config(void)
{
    int configfd;

    configfd = open("/tmp/looper.config", O_RDWR|O_CREAT|O_TRUNC);
    if (configfd < 0)
    {
	printf("Content-type: text/html\n\n");
	printf("<html><head><title>LP1 upload error</title></head><body>\n");
	printf("<H2>Upload error.</H2>\n");
	printf("</body></html>\n");
	fflush(stdout);
    }
    else
    {
	int nlines = 0;
	
	while (fgets(linebuf, sizeof(linebuf), stdin) != NULL)
	    if (strncmp(linebuf, "Content-Disposition:", 20) == 0)
		if (strstr(linebuf, "name=\"upload\"") != NULL)
		    break;
	
	while (fgets(linebuf, sizeof(linebuf), stdin) != NULL)
	    if (strcmp(linebuf, "\r\n") == 0)
		break;

	while (fgets(linebuf, sizeof(linebuf), stdin) != NULL)
	{
	    if (strncmp(linebuf, "-----", 5) == 0)
		break;
	    
	    nlines++;
	    write(configfd, linebuf, strlen(linebuf));
	}

	if (nlines > 0)
	{
	    int tofd;
	    int n;
	    
	    tofd = open("/j/looper.config", O_WRONLY|O_TRUNC|O_CREAT);
	    lseek(configfd, 0, SEEK_SET);
	    
	    do
	    {
		n = read(configfd, linebuf, sizeof(linebuf));
		if (n > 0)
		    write(tofd, linebuf, n);
	    }
	    while (n > 0);
	}

	close(configfd);
	
	printf("Content-type: text/html\n\n");
	printf("<html><head><title>LP1 upload</title></head><body>\n");
	printf("<H2>Upload completed.</H2>\n");
	printf("</body></html>\n");
	fflush(stdout);
    }
}

int main(int argc, char **argv)
{
    int i;

    if (sscanf(getenv("QUERY_STRING"), "save=%d", &i) == 1)
    {
	download_config();
    }
    if (sscanf(getenv("QUERY_STRING"), "restore=%d", &i) == 1)
    {
	upload_config();
    }

    return 0;
}
