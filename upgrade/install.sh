#! /bin/sh

#    Looperlative LP1 audio looper
#    Copyright (C) 2005-2013 Robert Amstadt dba Looperlative Audio Products
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; version 2 of the License only.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License along
#    with this program; if not, write to the Free Software Foundation, Inc.,
#    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
#    Proprietary licensing of the software as modified only by Looperlative
#    Audio Products is available.  Please contact Looperlative Audio Products
#    for details.
#
#    Email: licensing@looperlative.com
#
#    US mail:	Looperlative Audio Products
#    		6081 Meridian Ave. #70-315
#		San Jose, CA 95120

#
# Check CRC
#
cksum < userif > crc
if cmp crc userif.crc
then
    # Upgrade kernel if it is present in the upgrade
    if test -f uImage
    then
	cp uImage /dev/mtdblock1
	rm -f uImage
    fi

    cp looper.ko /j
    cp looper2.ko /j
    cp spi.ko /j
    cp userif /j
    chmod +x /j/userif

#    (cd /j; tar xf /mnt/lp1-www.tar)

    exit 0
fi

exit 1