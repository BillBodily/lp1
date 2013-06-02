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

LINUXDIR=/home/bob/R05R3-RC3/uClinux-dist

CC=bfin-uclinux-gcc
CFLAGS=-O2 -Wall -Dlinux -D__linux__ -Dunix -D__uClinux__ -DEMBED \
	-I$(LINUXDIR) -I$(LINUXDIR)/lib/uClibc/include \
	-fno-builtin
LDFLAGS=-Wl,-elf2flt
LDLIBS=-L$(LINUXDIR)/lib/uClibc -L$(LINUXDIR)/lib/uClibc/lib \
	-L$(LINUXDIR)/lib/uClibc/libm -L$(LINUXDIR)/lib/libnet \
	-L$(LINUXDIR)/lib/ligdes -L$(LINUXDIR)/lib/aes \
	-L$(LINUXDIR)/lib/libpcap -L$(LINUXDIR)/lib/zlib \
	-L$(LINUXDIR)/lib/libssl -L$(LINUXDIR)/lib/uClibc/libc \
	-L$(LINUXDIR)/lib/uClibc/libcrypt -lpthread

all: upgrade.cgi tracks.cgi upgrade_server looper.config
	cp looper.config /tftpboot
	cp upgrade.cgi /tftpboot
	cp tracks.cgi /tftpboot
	cp upgrade_server /tftpboot
	./build-install-set

looper.config: saveconfig.o Makefile
	$(CC) $(LDFLAGS) -o $@ saveconfig.o $(LDLIBS)

tracks.cgi: tracks.o Makefile
	$(CC) $(LDFLAGS) -o $@ tracks.o $(LDLIBS)

upgrade.cgi: upgrade.o Makefile
	$(CC) $(LDFLAGS) -o $@ upgrade.o $(LDLIBS)

upgrade_server: upgrade_server.o tcp.o Makefile
	$(CC) $(LDFLAGS) -o $@ upgrade_server.o tcp.o $(LDLIBS)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

