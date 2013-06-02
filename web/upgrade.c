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

int main(int argc, char **argv)
{
    char *s;
    
    s = getenv("SERVER_ADDR");

    printf("Content-type: text/html\n\n");
    printf("<html><head><title>LP1 software upgrade</title></head><body>\n");
    printf("<h1>LP1 upgrade page</h1>");
    printf("<p>");
    printf("<form action=\"http://%s:8080/doupgrade\" method=\"POST\" enctype=\"multipart/form-data\">", s);
    printf("Upload file: <INPUT type=\"file\" name=\"upgrade\" size=50>");
    printf("<INPUT TYPE=SUBMIT NAME=\"SEND\" VALUE=\"SEND\">");
    printf("</body></html>\n");

    return 0;
}
