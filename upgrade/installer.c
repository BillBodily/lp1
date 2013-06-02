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
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

char copy_buffer[1024];

char kernel_version[200] = "Linux version 2.6.12.1-BFIN-2005R4 (bob@delllinux) (gcc version 3.4.4 (Blackfin 05R4 20051205)) #12 Sat Nov 10 22:32:57 PST 2007";

int copy_file(char *src, char *dest)
{
    FILE *srcfp;
    FILE *destfp;
    size_t n;
    
    srcfp = fopen(src, "r");
    if (srcfp == NULL)
	return -1;
    
    destfp = fopen(dest, "w");
    if (destfp == NULL)
    {
	fclose(srcfp);
	return -1;
    }

    do
    {
	n = fread(copy_buffer, 1, sizeof(copy_buffer), srcfp);
	if (n > 0 && 
	    fwrite(copy_buffer, 1, n, destfp) != n)
	{
	    fclose(srcfp);
	    fclose(destfp);
	    return -1;
	}
    }
    while (n > 0);

    fclose(srcfp);
    fclose(destfp);
    return 0;
}

int check_file_size(char *src)
{
    struct stat fileinfo;
    FILE *fp;
    int crc;
    int filelength;

    if (stat(src, &fileinfo) != 0)
	return -1;
    
    sprintf(copy_buffer, "%s.crc", src);
    fp = fopen(copy_buffer, "r");
    if (fp == NULL)
	return -1;
    
    if (fscanf(fp, "%d %d", &crc, &filelength) != 2)
    {
	fclose(fp);
	return -1;
    }
    
    fclose(fp);
    
    if (fileinfo.st_size != filelength)
	return -1;
    else
	return 0;
}

int extract_tar_file(char *src)
{
    char filename[200];
    FILE *srcfp;
    int n;
    int i;
    FILE *destfp;
    mode_t mode;
    
    srcfp = fopen(src, "r");

    while (fread(copy_buffer, 512, 1, srcfp) == 1)
    {
	if (strlen(copy_buffer) == 0)
	    break;
	
	n = strtol(copy_buffer + 124, NULL, 8);
	mode = strtol(copy_buffer + 100, NULL, 8);

	if (copy_buffer[156] == '2')
	{
	    symlink(copy_buffer + 157, copy_buffer);
	}
	else if (copy_buffer[156] == '5')
	{
	    mkdir(copy_buffer, mode);
	}
	else
	{
	    strcpy(filename, copy_buffer);
	    destfp = fopen(filename, "w");
	    if (destfp)
	    {
		while (n > 0)
		{
		    i = fread(copy_buffer, 1, 512, srcfp);
		    if (n < 512)
		    {
			fwrite(copy_buffer, 1, n, destfp);
			n = 0;
		    }
		    else
		    {
			fwrite(copy_buffer, 1, i, destfp);
			n -= i;
		    }
		}
	    }
	    fclose(destfp);
	    chmod(filename, mode);
	}
    }

    fclose(srcfp);
    return 0;
}

int main(int argc, char **argv)
{
    if (check_file_size("looper2.ko") < 0 ||
	check_file_size("spi.ko") < 0 ||
	check_file_size("userif") < 0 ||
	check_file_size("lp1-www.tar") < 0)
    {
	return -1;
    }

    copy_file("looper2.ko", "/j/looper2.ko");
    copy_file("spi.ko", "/j/spi.ko");
    copy_file("userif", "/j/userif");

    chmod("/j/userif", 0755);

    if (check_file_size("uImage") == 0)
    {
	copy_file("uImage", "/dev/mtdblock1");
    }
    unlink("uImage");

    chdir("/j");
    unlink("html/testpage.html");
    unlink("html/upgrade.html");
    unlink("cgi-bin/test.cgi");
    extract_tar_file("/mnt/lp1-www.tar");
    
    return 0;
}
