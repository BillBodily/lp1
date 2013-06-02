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
# Add -DUSE_NEW_MIDI_HANDLER to CFLAGS to enable new midi handler code
#

LINUXDIR=../R05R4-RC2/uClinux-dist

EXEC = userif
OBJS = userif.o menus.o upgrade.o tracks.o config.o midisync.o udp.o \
	 midi_handler.o
SRCS = $(OBJS:.o=.c)

CC=bfin-uclinux-gcc
CFLAGS=-O2 -Wall -Dlinux -D__linux__ -Dunix -D__uClinux__ -DEMBED -D__USE_GNU\
	-I$(LINUXDIR) -I$(LINUXDIR)/lib/uClibc/include \
	-fno-builtin
LDFLAGS=-Wl,-elf2flt
LDLIBS=-L$(LINUXDIR)/lib/uClibc -L$(LINUXDIR)/lib/uClibc/lib \
	-L$(LINUXDIR)/lib/uClibc/libm -L$(LINUXDIR)/lib/libnet \
	-L$(LINUXDIR)/lib/ligdes -L$(LINUXDIR)/lib/aes \
	-L$(LINUXDIR)/lib/libpcap -L$(LINUXDIR)/lib/zlib \
	-L$(LINUXDIR)/lib/libssl -L$(LINUXDIR)/lib/uClibc/libc \
	-L$(LINUXDIR)/lib/uClibc/libcrypt -lpthread

DEPDIR = .deps
df = $(DEPDIR)/$(*F)
MAKEDEPEND = gcc -M $(CPPFLAGS) -o $(DEPDIR)/$*.d $<

%.o : %.c
	@$(MAKEDEPEND); \
	cp $(df).d $(df).P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	    -e '/^$$/ d' -e 's/$$/ :/' < $(df).d >> $(df).P; \
	rm -f $(df).d
	$(COMPILE.c) -o $@ $<

all: $(EXEC) midicat
	cp $(EXEC) /tftpboot
	cp midicat /tftpboot

$(EXEC): $(OBJS) Makefile
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

midicat: midicat.o Makefile
	$(CC) $(LDFLAGS) -o $@ midicat.o $(LDLIBS)

romfs:
	$(ROMFSINST) /bin/$(EXEC)

clean:
	-rm -f $(EXEC) *.elf *.gdb *.o

-include $(SRCS:%.c=$(DEPDIR)/%.P)

