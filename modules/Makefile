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

# MY_LINUX_DIR=/home/bob/R05R3-RC3/uClinux-dist/linux-2.6.x
MY_LINUX_DIR=../R05R4-RC2/uClinux-dist/linux-2.6.x

ifneq ($(KERNELRELEASE),)
# kbuild part of makefile
obj-m	:= looper2.o spi.o lp1sd.o
looper2-objs := sport.o loop_mem.o
# tx-objs := txtest.o loop_mem.o

# EXTRA_CFLAGS += -mcsync-anomaly -mspecld-anomaly -mno-cmov
# Set V=1 to see verbose compiler output
CFLAGS_sport.o = -DSPORT_MULTIPLE_CLOCKS -DSPORT_USE_L1_MEMORY

else

all::
	$(MAKE) -C $(MY_LINUX_DIR) SUBDIRS=`pwd` modules
	cp looper2.ko /tftpboot
	cp spi.ko /tftpboot
	cp lp1sd.ko /tftpboot

endif
