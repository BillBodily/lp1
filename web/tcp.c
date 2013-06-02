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
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include "tcp.h"

static int tcp_mastersocket = -1;

int tcp_initsock(char *service)
{
    struct servent *pse;
    struct sockaddr_in sin;
    int s;
    int opt;
    
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    
    if ((pse = getservbyname(service, "tcp")))
    {
	sin.sin_port = (u_short)pse->s_port;
    }
    else if ((sin.sin_port = htons((u_short)atoi(service))) == 0)
    {
	fprintf(stderr, "Can't find port for service %s\n", service);
	return -1;
    }
    
    s = socket(PF_INET, SOCK_STREAM, 6 /* tcp */);
    if (s < 0)
    {
	fprintf(stderr, "Can't create the socket\n");
	return -1;
    }

    opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
	fprintf(stderr, "Can't bind the socket\n");
	return -1;
    }
    
    if (listen(s, 10) < 0)
	return -1;

    tcp_mastersocket = s;
    return s;
}

int tcp_waitforconnect(int listenfd)
{
    struct sockaddr_in sin;
    int alen = sizeof(sin);
    int s;
    
    while ((s = accept(listenfd, (struct sockaddr *) &sin, &alen)) < 0)
	;
    
    return s;
}
