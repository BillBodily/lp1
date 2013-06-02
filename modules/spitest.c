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

#include <linux/module.h>
#include <linux/version.h>

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fcntl.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/timer.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/param.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <asm/blackfin.h>
#include <asm/simple_bf533_dma.h>

static struct timer_list periodic_timer;

static int spi_major = -1;
static struct file_operations spi_fops = 
{
};

static struct spi_s
{
    int dummy;
    unsigned long interrupts;
}
spi_info = 
{
    0, 0
};

static void disp_send_char(unsigned char c)
{
    unsigned short data;
    
    data = ((unsigned short) c << 8) | 0xc0;
    
    *pSPI_TDBR = data;
    while (*pSPI_STAT & 0x0008)
	;
}

static void disp_send_buffer(unsigned char *s, int len)
{
    int i;
    
    for (i = 0; i < len; i++)
    {
	printk("Sending %c\n", s[i]);
	disp_send_char(s[i]);
    }
}

static void test_pf2_spi(void)
{
    int i;
    
    *pSPI_BAUD = 2000;
    *pSPI_CTL = 0x5301;
    *pSPI_FLG = 0xff04;		/* Select PF2 */

    for (i = 0; i < 50; i++)
    {
	*pSPI_TDBR = 0xaa00;

	while (*pSPI_STAT & 0x0008)
	    ;

	*pSPI_TDBR = 0x5580;

	while (*pSPI_STAT & 0x0008)
	    ;

    }

    disp_send_buffer("\x1b\x40", 3);
    disp_send_char(0x0c);
    disp_send_buffer("Hello", 6);

    /* Key test */
    *pSPI_TDBR = 0x0040;	/* Turn on all 4 columns for testing */

    /* Enable inputs */
    *pFIO_DIR &= ~0xfe00;
    *pFIO_MASKA_C = 0xfe00;	/* Do not use as interrupts */
    *pFIO_MASKB_C = 0xfe00;	/* Do not use as interrupts */
    *pFIO_POLAR &= ~0xfe00;	/* Active high */
    *pFIO_EDGE &= ~0xfe00;	/* Level sensitive */
    *pFIO_INEN |= 0xfe00;	/* Enable input buffers */
}

static unsigned short midi_xfer(unsigned short data)
{
    *pSPI_TDBR = data;

    while (*pSPI_STAT & 0x0008)
	;
    while (!(*pSPI_STAT & 0x0001))
	;

    return *pSPI_RDBR;
}

static void midi_data_send(unsigned char data)
{
    while (!(midi_xfer(0x4000) & 0x4000))
	;
    
    midi_xfer(0x8000 | (unsigned short) data );
}

static void periodic_timeout(unsigned long data)
{
    periodic_timer.expires = jiffies + HZ;
    add_timer(&periodic_timer); 

    printk("%x\n", *pFIO_FLAG_D);

    spi_info.interrupts = 0;
}

static void test_midi(void)
{
    /* Initialize SPI */
    *pSPI_BAUD = 2000;
    *pSPI_CTL = 0x5101;
    *pSPI_FLG = 0xff08;		/* Select PF3 */

    printk("MIDI UART read config = %x\n", midi_xfer(0x4000));
    printk("MIDI UART write config = %x\n", midi_xfer(0xc003));
    printk("MIDI UART read config = %x\n", midi_xfer(0x4000));

    midi_data_send(0xb0);
    midi_data_send(0x07);
    midi_data_send(0x7f);
}

int init_module(void)
{
    spi_major = register_chrdev(0, "spi", &spi_fops);
    printk("Registered SPI device as %d\n", spi_major);

    printk("HZ is %d\n", HZ);

    /* Set default inactive values for PF2 and PF3 */
    *pFIO_DIR |= 0x000c;
    *pFIO_FLAG_S = 0x000c;

    test_pf2_spi();

/*    test_midi(); */

    init_timer(&periodic_timer);
    periodic_timer.function = periodic_timeout;
    periodic_timer.data = 0;
    periodic_timer.expires = jiffies + HZ;
    add_timer(&periodic_timer);

    return 0;
}


void cleanup_module(void)
{
    /* Delete system timer */
    del_timer(&periodic_timer);

    unregister_chrdev(spi_major, "spi");
}

MODULE_LICENSE("Looperlative Audio Products Proprietary");
MODULE_AUTHOR("Robert Amstadt <bob@looperlative.com>");
MODULE_DESCRIPTION("Looperlative LP1 looping controller");
