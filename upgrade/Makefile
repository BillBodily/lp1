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

LINUXDIR=../R05R4-RC2/uClinux-dist

DISPECHOEXEC = dispecho
DISPECHOOBJS = dispecho.o

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

all: cleanstages buildstage1

buildstage1: stage1 stage1/upgrade.sh stage1/cksum stage1/dispecho buildstage2
	(cd stage2; tar cf ../stage1/lp1stage2.tar *)
	(cd stage1; split -b 64k lp1stage2.tar lp1stage2s.)
	(cd stage1; ls lp1stage2s.* > upgradefiles)
	(cd stage1; cksum < lp1stage2.tar > upgradecrc)
	(cd stage1; tar cf lp1upgrade.tar cksum upgrade.sh upgradefiles upgradecrc dispecho)

buildstage2: stage2 stage2/looper2.ko stage2/spi.ko stage2/userif stage2/install.sh stage2/lp1-www.tar stage2/installer
# buildstage2: stage2 stage2/uImage stage2/looper2.ko stage2/spi.ko stage2/userif stage2/install.sh stage2/lp1-www.tar stage2/installer

stage1/upgrade.sh: upgrade.sh
	cp upgrade.sh stage1
	chmod +x stage1/upgrade.sh

stage1/cksum: ../R05R4-RC2/uClinux-dist/romfs/bin/cksum
	cp ../R05R4-RC2/uClinux-dist/romfs/bin/cksum stage1
	chmod +x stage1/cksum

stage1/dispecho: dispecho
	cp dispecho stage1
	chmod +x stage1/dispecho

stage2/uImage: ../../R05R4-RC2/uClinux-dist/images/uImage
	cp ../../R05R4-RC2/uClinux-dist/images/uImage stage2
	(cd stage2; cksum < uImage > uImage.crc)

stage2/looper2.ko: ../modules/looper2.ko
	cp ../modules/looper2.ko stage2
	(cd stage2; cksum < looper2.ko > looper2.ko.crc)

stage2/spi.ko: ../modules/spi.ko
	cp ../modules/spi.ko stage2
	(cd stage2; cksum < spi.ko > spi.ko.crc)

stage2/userif: ../userif/userif
	cp ../userif/userif stage2
	chmod +x stage2/userif
	(cd stage2; cksum < userif > userif.crc)

stage2/install.sh: install.sh
	cp install.sh stage2/install.sh
	chmod +x stage2/install.sh
	(cd stage2; cksum < install.sh > install.sh.crc)

stage2/installer: installer
	cp installer stage2/installer
	chmod +x stage2/installer
	(cd stage2; cksum < installer > installer.crc)

stage2/lp1-www.tar: ../web/lp1-www.tar
	cp ../web/lp1-www.tar stage2/lp1-www.tar
	chmod +x stage2/lp1-www.tar
	(cd stage2; cksum < lp1-www.tar > lp1-www.tar.crc)

stage1:
	-mkdir stage1

stage2:
	-mkdir stage2

$(DISPECHOEXEC): $(DISPECHOOBJS) Makefile
	$(CC) $(LDFLAGS) -o $@ $(DISPECHOOBJS) $(LDLIBS)

installer: installer.o
	$(CC) $(LDFLAGS) -o $@ installer.o $(LDLIBS)

romfs:
	$(ROMFSINST) /bin/$(DISPECHOEXEC)

clean:
	-rm -f $(DISPECHOEXEC) *.elf *.gdb *.o
	-rm -rf stage1 stage2

cleanstages:
	-rm -rf stage1 stage2

