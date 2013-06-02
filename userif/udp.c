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
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include "userif.h"
#include "menus.h"
#include "config.h"
#include "tracks.h"
#include "../modules/sport.h"

extern char *strcasestr (__const char *__haystack, __const char *__needle);
void queue_udp_command(int func, int arg);

extern struct sport_loop_status_s last_ls;
extern struct menu_item_s main_menu_items[];
extern struct config_functions_s looper_functions[];
extern int track_current_track;
extern pthread_mutex_t userif_lock;
extern pthread_cond_t button_cond;

static char udp_buffer[1024];
static char send_buffer[4096];

void udp_return_commandlist(int s, struct sockaddr_in *sinp)
{
    int i;
    int f;
    int n;

    i = 0;

    i += sprintf(send_buffer + i, "<commandlist>\n");

    for (f = 0; looper_functions[f].min_id >= 0; f++)
    {
	if (looper_functions[f].min_id == looper_functions[f].max_id)
	{
	    i += sprintf(send_buffer + i, "  <command>%s</command>\n",
			 looper_functions[f].name);
	}
	else
	{
	    for (n = 0; 
		 n <= looper_functions[f].max_id - looper_functions[f].min_id;
		 n++)
	    {
		i += sprintf(send_buffer + i, "  <command>%s %d</command>\n",
			     looper_functions[f].name, n + 1);
	    }
	}
    }

    i += sprintf(send_buffer + i, "</commandlist>\n");

    sendto(s, send_buffer, i + 1, 0, 
	   (struct sockaddr *) sinp, sizeof(*sinp));
}

void udp_return_status(int s, struct sockaddr_in *sinp)
{
    int i;
    int track;
    
    i = 0;

    i += sprintf(send_buffer + i, "<status>\n");

    for (track = 1; track <= 8; track++)
    {
	/*
	  <track1>
	  <state>playing</state>
	  <length>0.000<length>
	  <position>0.000</position>
	  <level>-000</level>
	  <pan>-000</pan>
	  <feedback>000</feedback>
	  <selected>yes</selected>
	  </track1>
	*/
		
	i += sprintf(send_buffer + i, "  <track%d>\n", track);

	i += sprintf(send_buffer + i, "    <state>");
	switch((last_ls.track_states >> ((track - 1) * 4)) & 0xf)
	{
	  case SPORT_TRACK_STATUS_RECORDING:
	    i += sprintf(send_buffer + i, "recording"); break;
	  case SPORT_TRACK_STATUS_OVERDUBBING:
	    i += sprintf(send_buffer + i, "overdubbing"); break;
	  case SPORT_TRACK_STATUS_STOPPED:
	    i += sprintf(send_buffer + i, "stopped"); break;
	  case SPORT_TRACK_STATUS_PLAYING:
	    i += sprintf(send_buffer + i, "playing"); break;
	  case SPORT_TRACK_STATUS_REPLACING:
	    i += sprintf(send_buffer + i, "replacing"); break;
	  default:
	    i += sprintf(send_buffer + i, "empty"); break;
	}
	i += sprintf(send_buffer + i, "</state>\n");

	i += sprintf(send_buffer + i, "    <length>%d.%d</length>\n",
		     last_ls.track_length[track-1] / 352800,
		     ((last_ls.track_length[track-1] * 10) / 3528) % 1000);

	i += sprintf(send_buffer + i, "    <position>%d.%d</position>\n",
		     last_ls.track_position[track-1] / 352800,
		     ((last_ls.track_position[track-1] * 10) / 3528) % 1000);

	i += sprintf(send_buffer + i, "    <level>%+04d</level>\n",
		     looper_config.track_gains[track-1]);

	i += sprintf(send_buffer + i, "    <pan>%+04d</pan>\n",
		     looper_config.track_pans[track-1]);

	i += sprintf(send_buffer + i, "    <feedback>%03d</feedback>\n",
		     looper_config.track_feedbacks[track-1]);

	if (track == track_current_track)
	    i += sprintf(send_buffer + i, "<selected>yes</selected>\n");
	else
	    i += sprintf(send_buffer + i, "<selected>no</selected>\n");

	i += sprintf(send_buffer + i, "  </track%d>\n", track);
    }

    i += sprintf(send_buffer + i, "</status>\n", track);

    sendto(s, send_buffer, i + 1, 0, 
	   (struct sockaddr *) sinp, sizeof(*sinp));
}

char *udp_process_query(int s, struct sockaddr_in *sinp, char *p)
{
    p += strspn(p, " \r\n\t");
    if (strncasecmp(p, "id", 2) == 0 && !isalpha(p[2]))
    {
	sprintf(send_buffer, "<id>LP1 %s</id>\n", 
		main_menu_items[0].displaystr);
	sendto(s, send_buffer, strlen(send_buffer) + 1, 0, 
	       (struct sockaddr *) sinp, sizeof(*sinp));
    }
    else if (strncasecmp(p, "status", 6) == 0 && !isalpha(p[6]))
    {
	udp_return_status(s, sinp);
    }
    else if (strncasecmp(p, "commandlist", 11) == 0 && !isalpha(p[11]))
    {
	udp_return_commandlist(s, sinp);
    }

    p = strcasestr(p, "</query>");
    if (p != NULL)
	p += 8;

    return p;
}

char *udp_process_command(int s, struct sockaddr_in *sinp, char *p)
{
    int f;
    int arg;
    
    p += strspn(p, " \r\n\t");

    for (f = 0; looper_functions[f].min_id >= 0; f++)
    {
	if (strncasecmp(p, looper_functions[f].name, 
			strlen(looper_functions[f].name)) == 0)
	{
	    arg = looper_functions[f].arg;
	    
	    p += strlen(looper_functions[f].name);
	    p += strspn(p, " \r\n\t");
	    if (isdigit(*p) || *p == '-')
	    {
		arg = atoi(p);
	    }

//	    printf("UDP queue command %d, %d\n", f, arg);

	    queue_udp_command(f, arg);
	    
	    break;
	}
    }

    p = strcasestr(p, "</command>");
    if (p != NULL)
	p += 10;

    return p;
}

void *udp_handler(void *arg)
{
    struct sockaddr_in sin;
    char *p;
    int s;
    int n;
    socklen_t recvsinlen;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
	return NULL;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(5667);
    
    if (bind(s, (struct sockaddr *) &sin, sizeof(sin)))
	return NULL;

    recvsinlen = sizeof(sin);
    while ((n = recvfrom(s, udp_buffer, sizeof(udp_buffer), MSG_TRUNC,
			 (struct sockaddr *) &sin, &recvsinlen)) > 0)
    {
	p = udp_buffer;
	if (n >= sizeof(udp_buffer))
	    udp_buffer[sizeof(udp_buffer) - 1] = '\0';
	else
	    udp_buffer[n] = '\0';

//	printf("UDP RX: %s\n", p);
//	printf("UDP RX from %x %d\n", 
//	       ntohl(sin.sin_addr.s_addr), ntohs(sin.sin_port));
	
	while (p != NULL && (p = index(p, '<')) != NULL)
	{
	    p++;
	    if (strncasecmp(p, "query>", 6) == 0)
	    {
		p = udp_process_query(s, &sin, p + 6);
	    }
	    else if (strncasecmp(p, "command>", 8) == 0)
	    {
		p = udp_process_command(s, &sin, p + 8);
	    }

	}

	recvsinlen = sizeof(sin);
    }

    return NULL;
}
