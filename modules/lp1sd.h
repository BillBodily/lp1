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

#ifndef __lp1sd_h__
#define __lp1sd_h__

#ifndef PDEBUG
#  ifdef DEBUG_lp1sd
#    ifdef __KERNEL__
#      define PDEBUG(fmt, args...) printk (fmt , ## args)
#    else
#      define PDEBUG(fmt, args...) fprintf (stderr, fmt , ## args)
#    endif
#  else
#    define PDEBUG(fmt, args...)
#  endif
#endif

#ifndef PDEBUGG
#  define PDEBUGG(fmt, args...)
#endif

#ifndef Static
#  ifdef DEBUG_lp1sd
#    define Static /* nothing */
#  else
#    define Static static
#  endif
#endif

#endif /* __lp1sd_h__ */
