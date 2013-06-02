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
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/tty.h>
#include <linux/termios.h>
#include <linux/delay.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/ioctl.h>
#include <linux/hdreg.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/param.h>
#include <asm/uaccess.h>

#include <asm/blackfin.h>

#include "spi.h"
#include "lp1sd.h"

#define LP1_MODULE_DEBUG

#ifdef LP1_MODULE_DEBUG
#define DDEBUG(x)	printk x
#else
#define DDEBUG(x)
#endif

#define BAUD_SLOW	158	/* 79 MHz / (2 * 250k baud) */
#define BAUD_FAST	4	/* 79 MHz / (2 * 10M baud) */
//#define BAUD_PIC	80
#define BAUD_PIC	160

#define SPI_MAJOR	252

// Driver variables
extern struct block_device_operations lp1sd_bdops;
struct gendisk *lp1sd_gd = NULL;
struct request_queue *lp1sd_queue = NULL;
int lp1sd_major = 0;
int spi_major = -1;

// Media control
int lp1sd_media_detect = 0;
int lp1sd_media_changed = 1;

// Geometry
int lp1sd_blk_sizes[1] = {0};
int lp1sd_blksize_sizes[1] = {0};
int lp1sd_hardsect_sizes[1] = {0};
int lp1sd_sectors = 0;

// Locks and timers
spinlock_t lp1sd_lock = SPIN_LOCK_UNLOCKED;
spinlock_t spi_irq_lock = SPIN_LOCK_UNLOCKED;

volatile short spi_request_stall = 0;
volatile short spi_stalled = 1;

unsigned char lp1sd_spi_io(unsigned char data_out);
unsigned char lp1sd_send_command(unsigned char *cmd);

// Front panel routines and variables
int spi_open(struct inode *inode, struct file *filp);
int spi_close(struct inode *inode, struct file *filp);

int fpanel_open(struct inode *inode, struct file *filp);
int fpanel_close(struct inode *inode, struct file *filp);
int fpanel_write(struct file *filp, const char *bug, 
		 size_t count, loff_t *offp);
int fpanel_read(struct file *filp, char *bug, size_t count, loff_t *offp);
int fpanel_ioctl(struct inode *inode, struct file *filp, 
		 uint cmd, unsigned long arg);

int midi_open(struct inode *inode, struct file *filp);
int midi_close(struct inode *inode, struct file *filp);
int midi_read(struct file *filp, char *bug, size_t count, loff_t *offp);
int midi_write(struct file *filp, const char *bug, size_t count, loff_t *offp);
int midi_ioctl(struct inode *inode, struct file *filp, 
	       uint cmd, unsigned long arg);

static struct file_operations spi_fops = 
{
    open:	spi_open,
    release:	spi_close
};
static struct file_operations midi_fops = 
{
    open:	midi_open,
    release:	midi_close,
    read:	midi_read,
    write:	midi_write,
    ioctl:	midi_ioctl
};
static struct file_operations fpanel_fops = 
{
    open:	fpanel_open,
    release:	fpanel_close,
    read:	fpanel_read,
    write:	fpanel_write,
    ioctl:	fpanel_ioctl
};

static struct spi_s spi_info;
DECLARE_WAIT_QUEUE_HEAD (midi_input_wait);
DECLARE_WAIT_QUEUE_HEAD (fpanel_input_wait);
DECLARE_WAIT_QUEUE_HEAD (fpanel_output_wait);

typedef int (*midi_clock_handler_t)(unsigned long arg);
typedef short (*midi_clock_checker_t)(void);
static midi_clock_handler_t midi_clock_handler = NULL;
static midi_clock_checker_t midi_clock_checker = NULL;

void lp1sd_prepare_read_sector(int sector);
void lp1sd_prepare_write_sector(struct request *req, int sector);

#define SPI_MODE_IDLE		0
#define SPI_MODE_SEND_COMMAND	1
#define SPI_MODE_WAIT_RESPONSE	2
#define SPI_MODE_SEND_DATA	3
#define SPI_MODE_RECV_DATA	4
#define SPI_MODE_WAIT_SOD_TOKEN	5
#define SPI_MODE_SEND_SOD_TOKEN	6
#define SPI_MODE_WAIT_WRITE	7
#define SPI_MODE_PAUSE		8
#define SPI_MODE_SEND_PIC	9
#define SPI_MODE_WAIT_PIC	10
#define SPI_MODE_RECV_PIC	11

#define SPI_DATADIRECTION_SEND	0
#define SPI_DATADIRECTION_RECV	1

static unsigned char lp1sd_command_buffer[10];
static unsigned char lp1sd_command_response;
static unsigned char lp1sd_command_data[1024];
static short lp1sd_command_length = 0;
static short lp1sd_data_length = 0;
static short lp1sd_data_direction = 0;

struct request *lp1sd_current_req = NULL;
volatile int lp1sd_current_sector_n = 0;
volatile int lp1sd_request_done = 1;

static unsigned char pic_command_buffer[10];
static unsigned char pic_response_buffer[10];
volatile short pic_command_length = 0;
volatile short pic_response_length = 0;
static short pic_delay = 0;

static unsigned char spi_command_buffer[10];
static unsigned char spi_command_response;
static unsigned char spi_command_data[1024];
static short spi_command_length = 0;
static short spi_data_length = 0;
static short spi_data_direction = 0;

volatile short spi_command_mode = SPI_MODE_IDLE;
volatile short spi_buffer_idx = 0;
volatile long spi_write_wait = 0;

unsigned char spi_display_q[256];
volatile unsigned char spi_display_q_in = 0;
volatile unsigned char spi_display_q_out = 0;

volatile struct track_timer_s track_timer = { -1, -1, -1 };
volatile struct volume_bar_s volume_bars;
volatile short send_volume_bars = 0;

static short display_section = 0;	/* Display section being written to */
static short spi_display_section = 0;	/* Section being xmitted over SPI */

volatile unsigned char spi_transmit_leds = 1;

static irqreturn_t spi_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    struct spi_s *s = &spi_info;
    unsigned short in_data;
    unsigned short status;
    unsigned short mcc;
    unsigned short i;
    unsigned char b;
    unsigned long flags;
    
    spin_lock_irqsave(&spi_irq_lock, flags);

    status = *pSPI_STAT;
    in_data = *pSPI_SHADOW;

    /* If transfer in progress then don't do anything */
    if (!(status & 0x0001))
    {
	spin_unlock_irqrestore(&spi_irq_lock, flags);
	return IRQ_HANDLED;
    }

    switch (spi_command_mode)
    {
      case SPI_MODE_PAUSE:
	*pFIO_FLAG_S = 0x0014;
	*pSPI_BAUD = BAUD_PIC;
	*pSPI_FLG = 0x0800;
	*pSPI_TDBR = 0xff;
	spi_command_mode = SPI_MODE_IDLE;
	break;
	
      case SPI_MODE_IDLE:
	*pFIO_FLAG_S = 0x0014;
	*pSPI_BAUD = BAUD_PIC;
	*pSPI_FLG = 0x0800;
	*pSPI_TDBR = 0xff;

	if (lp1sd_current_req != NULL)
	{
	    memcpy(spi_command_buffer, lp1sd_command_buffer, 
		   lp1sd_command_length);
	    if (lp1sd_data_direction == SPI_DATADIRECTION_SEND)
	    {
		memcpy(spi_command_data, lp1sd_command_data, 
		       lp1sd_data_length);
	    }
	    spi_command_length = lp1sd_command_length;
	    spi_data_length = lp1sd_data_length;
	    spi_data_direction = lp1sd_data_direction;
	    
	    spi_command_mode = SPI_MODE_SEND_COMMAND;
	    spi_buffer_idx = 0;
	}
	else if (pic_command_length > 0)
	{
	    spi_command_mode = SPI_MODE_SEND_PIC;
	    spi_buffer_idx = 0;
	}
	else if (spi_display_q_in != spi_display_q_out)
	{
	    unsigned char b = spi_display_q[spi_display_q_out];
	    
	    if (b & 0x80)
	    {
		spi_display_section = b & 0x3;
		if (b & 0x40)
		    spi_display_q_out++;
	    }
	    
	    pic_command_buffer[0] = 0x95;
	    pic_command_buffer[1] = ((2 + spi_display_section) << 4);
	    
	    for (i = 0; i < 4; i++)
	    {
		b = spi_display_q[spi_display_q_out++];

		if (b & 0x80)
		    break;
		
		pic_command_buffer[i+2] = b;
	    }

	    if (spi_display_section > 0)
	    {
		pic_command_buffer[1] |= i;
		for ( ; i < 4; i++)
		    pic_command_buffer[i+2] = 0;
		pic_command_buffer[6] = (pic_command_buffer[1] +
					 pic_command_buffer[2] +
					 pic_command_buffer[3] +
					 pic_command_buffer[4] +
					 pic_command_buffer[5]);

#if 0
		DDEBUG(("PIC: %x %x %x %x %x %x %x\n",
		       pic_command_buffer[0],
		       pic_command_buffer[1],
		       pic_command_buffer[2],
		       pic_command_buffer[3],
		       pic_command_buffer[4],
		       pic_command_buffer[5],
		       pic_command_buffer[6]));
#endif

		pic_command_length = 7;
//		pic_delay = 200;
	    }
	}
	else if (track_timer.x >= 0)
	{
	    pic_command_buffer[0] = 0x95;
	    pic_command_buffer[1] = 0x54;
	    pic_command_buffer[2] = track_timer.x;
	    pic_command_buffer[3] = track_timer.y;
	    pic_command_buffer[4] = (unsigned char) (track_timer.value >> 8);
	    pic_command_buffer[5] = (unsigned char) track_timer.value;
	    
	    pic_command_buffer[6] = (pic_command_buffer[1] +
				     pic_command_buffer[2] +
				     pic_command_buffer[3] +
				     pic_command_buffer[4] +
				     pic_command_buffer[5]);

	    track_timer.x = -1;

#if 0
	    DDEBUG(("PIC: %x %x %x %x %x %x %x\n",
		   pic_command_buffer[0],
		   pic_command_buffer[1],
		   pic_command_buffer[2],
		   pic_command_buffer[3],
		   pic_command_buffer[4],
		   pic_command_buffer[5],
		   pic_command_buffer[6]));
#endif

	    pic_command_length = 7;
	}
	else if (send_volume_bars)
	{
	    pic_command_buffer[0] = 0x95;
	    pic_command_buffer[1] = 0x64;
	    pic_command_buffer[2] = (unsigned char) (volume_bars.in_mask >> 8);
	    pic_command_buffer[3] = (unsigned char) volume_bars.in_mask;
	    pic_command_buffer[4] = (unsigned char)(volume_bars.out_mask >> 8);
	    pic_command_buffer[5] = (unsigned char) volume_bars.out_mask;
	    
	    pic_command_buffer[6] = (pic_command_buffer[1] +
				     pic_command_buffer[2] +
				     pic_command_buffer[3] +
				     pic_command_buffer[4] +
				     pic_command_buffer[5]);

	    send_volume_bars = 0;

#if 0
	    DDEBUG(("PIC: %x %x %x %x %x %x %x\n",
		   pic_command_buffer[0],
		   pic_command_buffer[1],
		   pic_command_buffer[2],
		   pic_command_buffer[3],
		   pic_command_buffer[4],
		   pic_command_buffer[5],
		   pic_command_buffer[6]));
#endif

	    pic_command_length = 7;
	}
	else if (midi_clock_checker != NULL && 
		 (mcc = (*midi_clock_checker)()) != 0)
	{
	    pic_command_buffer[0] = 0x95;
	    pic_command_buffer[1] = 0x11;
	    if (mcc == 2)
		pic_command_buffer[2] = 0xfa;
	    else if (mcc == 3)
		pic_command_buffer[2] = 0xfc;
	    else
		pic_command_buffer[2] = 0xf8;
	    pic_command_buffer[3] = 0x00;
	    pic_command_buffer[4] = 0x00;
	    pic_command_buffer[5] = 0x00;
	    pic_command_buffer[6] = (pic_command_buffer[1] +
				     pic_command_buffer[2] +
				     pic_command_buffer[3] +
				     pic_command_buffer[4] +
				     pic_command_buffer[5]);
	    pic_command_length = 7;
	}
	else /* if (spi_transmit_leds) */
	{
	    spi_transmit_leds = 0;
	    
	    // Send LED status if nothing else to do.
	    pic_command_buffer[0] = 0x95;
	    pic_command_buffer[1] = 0x24;
	    pic_command_buffer[2] = (unsigned char) s->led_image[0];
	    pic_command_buffer[3] = (unsigned char) (s->led_image[0] >> 8);
	    pic_command_buffer[4] = (unsigned char) s->led_image[1];
	    pic_command_buffer[5] = (unsigned char) (s->led_image[1] >> 8);
	    pic_command_buffer[6] = (pic_command_buffer[1] +
				     pic_command_buffer[2] +
				     pic_command_buffer[3] +
				     pic_command_buffer[4] +
				     pic_command_buffer[5]);
	    pic_command_length = 7;
	}
#if 0
	else
	{
	    pic_command_buffer[0] = 0x96;
	    pic_command_length = 1;
	}
#endif
		
	break;
	
      case SPI_MODE_SEND_COMMAND:
	*pFIO_FLAG_C = 0x0004;
	*pSPI_BAUD = BAUD_FAST;
	*pSPI_TDBR = spi_command_buffer[spi_buffer_idx++];
	if (spi_buffer_idx >= spi_command_length)
	{
	    spi_command_mode = SPI_MODE_WAIT_RESPONSE;
	    spi_buffer_idx = 0;
	}
	break;

      case SPI_MODE_WAIT_RESPONSE:
	*pSPI_TDBR = 0xff;
	if (in_data == 0x00)
	{
	    spi_command_response = in_data;
	    spi_buffer_idx = 0;
	    if (spi_data_direction == SPI_DATADIRECTION_RECV)
		spi_command_mode = SPI_MODE_WAIT_SOD_TOKEN;
	    else
		spi_command_mode = SPI_MODE_SEND_SOD_TOKEN;
	}
	else if (in_data != 0xff)
	{
	    spi_command_response = in_data;
	    spi_buffer_idx = 0;
	    spi_command_mode = SPI_MODE_PAUSE;

	    if (lp1sd_current_req)
	    {
//		DDEBUG(("FAIL REQUEST recv'ed %x\n", in_data));
		end_request(lp1sd_current_req, 0);
		lp1sd_current_req = NULL;
		lp1sd_request_done = 1;
	    }
	}
	else if (++spi_buffer_idx > 10)
	{
	    spi_buffer_idx = 0;
	    spi_command_mode = SPI_MODE_PAUSE;

	    if (lp1sd_current_req)
	    {
//		DDEBUG(("FAIL REQUEST response too slow\n"));
		end_request(lp1sd_current_req, 0);
		lp1sd_current_req = NULL;
		lp1sd_request_done = 1;
	    }
	}

	break;

      case SPI_MODE_WAIT_SOD_TOKEN:
	*pSPI_TDBR = 0xff;
	if (in_data == 0xfe)
	{
	    spi_buffer_idx = 0;
	    spi_command_response = 0;
	    spi_command_mode = SPI_MODE_RECV_DATA;
	}
	else if (++spi_buffer_idx > 10000)
	{
	    spi_command_response = 0xfe;
	    spi_buffer_idx = 0;
	    spi_command_mode = SPI_MODE_PAUSE;

	    if (lp1sd_current_req)
	    {
//		DDEBUG(("FAIL REQUEST no SOD token\n"));
		end_request(lp1sd_current_req, 0);
		lp1sd_current_req = NULL;
		lp1sd_request_done = 1;
	    }
	}
	break;

      case SPI_MODE_RECV_DATA:
	*pSPI_TDBR = 0xff;
	spi_command_data[spi_buffer_idx++] = in_data;
	if (spi_buffer_idx >= spi_data_length)
	{
	    spi_buffer_idx = 0;
	    spi_command_mode = SPI_MODE_PAUSE;

	    if (lp1sd_current_req)
	    {
		unsigned char *p = lp1sd_current_req->buffer;
		
		p += (lp1sd_current_sector_n * 512);
		lp1sd_current_sector_n++;
		
		memcpy(p, spi_command_data, 512);

		if (lp1sd_current_sector_n >= 
		    lp1sd_current_req->current_nr_sectors)
		{
		    end_request(lp1sd_current_req, 1);
		    lp1sd_current_req = NULL;
		    lp1sd_request_done = 1;
		}
		else
		{
		    lp1sd_prepare_read_sector(lp1sd_current_req->sector + 
					      lp1sd_current_sector_n);
		    spi_command_mode = SPI_MODE_IDLE;
		}
	    }
	}
	break;

      case SPI_MODE_SEND_SOD_TOKEN:
	*pSPI_TDBR = 0xfe;
	spi_buffer_idx = 0;
	spi_command_mode = SPI_MODE_SEND_DATA;
	break;
	
      case SPI_MODE_SEND_DATA:
	*pSPI_TDBR = spi_command_data[spi_buffer_idx++];
	if (spi_buffer_idx >= spi_data_length)
	{
	    spi_buffer_idx = 0;
	    spi_write_wait = 0;
	    spi_command_response = 0;
	    spi_command_mode = SPI_MODE_WAIT_WRITE;
	}
	break;

      case SPI_MODE_WAIT_WRITE:
	*pSPI_TDBR = 0xff;
	if (++spi_write_wait > 1 && in_data == 0xff)
	{
	    spi_write_wait = 0;
	    spi_command_mode = SPI_MODE_PAUSE;

	    if (lp1sd_current_req)
	    {
		if (++lp1sd_current_sector_n >= 
		    lp1sd_current_req->current_nr_sectors)
		{
		    end_request(lp1sd_current_req, 1);
		    lp1sd_current_req = NULL;
		    lp1sd_request_done = 1;
		}
		else
		{
		    lp1sd_prepare_write_sector(lp1sd_current_req, 
					       lp1sd_current_req->sector + 
					       lp1sd_current_sector_n);
		    spi_command_mode = SPI_MODE_IDLE;
		}
	    }
	}
	else if (spi_write_wait > 1000000)
	{
	    spi_write_wait = 0;
	    spi_command_mode = SPI_MODE_PAUSE;

	    if (lp1sd_current_req)
	    {
//		DDEBUG(("lp1sd: end request\n"));
		end_request(lp1sd_current_req, 0);
		lp1sd_current_req = NULL;
		lp1sd_request_done = 1;
	    }
	}
	break;

      case SPI_MODE_SEND_PIC:
	if (pic_delay > 0)
	    pic_delay--;

	if (pic_delay > 0)
	{
	    *pSPI_FLG = 0x0800;
	    *pSPI_TDBR = 0xff;
	}
	else 
	{
	    *pSPI_FLG = 0x0808;
	    *pSPI_TDBR = pic_command_buffer[spi_buffer_idx++];
	
	    if (spi_buffer_idx >= pic_command_length)
	    {
		spi_command_mode = SPI_MODE_WAIT_PIC;
		spi_buffer_idx = 0;
	    }
	}
	
	break;

      case SPI_MODE_WAIT_PIC:
	pic_response_length = 0;
	*pSPI_FLG = 0x0808;
	*pSPI_TDBR = 0xff;

	if (in_data == 0x00)
	{
	    // Command completed.  No return message.
//	    DDEBUG(("pic command completed successfully without return.\n"));
	    spi_buffer_idx = 0;
	    pic_command_length = 0;
	    spi_command_mode = SPI_MODE_PAUSE;
	}
	else if (in_data == 0x01)
	{
	    // Command completed.  Return message.
	    spi_buffer_idx = 0;
	    spi_command_mode = SPI_MODE_RECV_PIC;
	}
	else if (in_data != 0xff)
	{
	    // Command error.
	    DDEBUG(("pic command completed with error %x.\n", in_data));
	    spi_buffer_idx = 0;
	    pic_command_length = 0;
	    spi_command_mode = SPI_MODE_PAUSE;
	}
	else if (++spi_buffer_idx > 200)
	{
	    // Command did not complete.
	    DDEBUG(("pic command timed out.\n"));
	    pic_command_length = 0;
	    spi_command_mode = SPI_MODE_PAUSE;
	}
	break;

      case SPI_MODE_RECV_PIC:
	*pSPI_FLG = 0x0808;
	*pSPI_TDBR = 0xff;

	pic_response_buffer[pic_response_length++] = in_data;
	if (pic_response_length >= 7 && pic_response_buffer[0] == 0x95)
	{
	    if (pic_response_buffer[1] == 0x84)
	    {
		unsigned short scancode;
		
		scancode = pic_response_buffer[3];
		scancode <<= 8;
		scancode |= pic_response_buffer[2];
		
		s->key_scancode = scancode;

		s->encoder_change = (int) (signed char) pic_response_buffer[4];
		wake_up_interruptible(&fpanel_input_wait);
	    }
	    else if ((pic_response_buffer[1] & 0xf0) == 0x90)
	    {
		for (i = 0; i < (pic_response_buffer[1] & 0x0f); i++)
		{
		    b = pic_response_buffer[2 + i];
		    if (b == 0xf8 && midi_clock_handler != NULL)
			(*midi_clock_handler)(0);
		    else
		    {
			s->midi_in_q[s->midi_in_q_in++] = b;
			if (s->midi_in_q_in >= 
			    (sizeof(s->midi_in_q)/sizeof(s->midi_in_q[0])))
			    s->midi_in_q_in = 0;
			wake_up_interruptible(&midi_input_wait);
		    }
		}
	    }

	    pic_command_length = 0;
	    spi_command_mode = SPI_MODE_PAUSE;
	}

	break;

    }

    *pSPI_RDBR;		// Start next cycle

    spin_unlock_irqrestore(&spi_irq_lock, flags);
    return IRQ_HANDLED;
}

int spi_open(struct inode *inode, struct file *filp)
{
    if (MINOR(inode->i_rdev) == 0)
	return fpanel_open(inode, filp);
    else if (MINOR(inode->i_rdev) == 1)
	return midi_open(inode, filp);
    else
	return -ENODEV;
}

int spi_close(struct inode *inode, struct file *filp)
{
    return -EIO;
}

int fpanel_open(struct inode *inode, struct file *filp)
{
    filp->f_op = &fpanel_fops;

    return 0;
}

int fpanel_close(struct inode *inode, struct file *filp)
{
    return 0;
}

int fpanel_write(struct file *filp, const char *buf, 
		 size_t count, loff_t *offp)
{
    int n = 0;
    unsigned char nqueue;
    unsigned char want;
    short new_section = -1;
    unsigned char q_in;
    
    if (count > 100)
	return -EINVAL;

    spin_lock(&lp1sd_lock);

    /* Wait for enough space in the queue */
    nqueue = spi_display_q_out - spi_display_q_in - 1;
    want = 2 * count + 50;

    while (nqueue <= want)
    {
	spin_unlock(&lp1sd_lock);
	interruptible_sleep_on(&fpanel_output_wait);
	spin_lock(&lp1sd_lock);

	nqueue = spi_display_q_out - spi_display_q_in - 1;
    }

    if (buf[0] == '\x1f')
    {
	if (buf[1] == '\x28' && buf[2] == '\x77' && buf[3] == '\x01')
	{
	    n = 5;
	    new_section = 0;
	}
	else if (buf[1] == '\x24')
	{
	    if (buf[2] == '\x36')
		new_section = 0;
	    else if (buf[4] == '\x01')
		new_section = 2;
	    else
		new_section = 1;
	    n = 6;
	}
    }

    /* Queue data to display */
    if (new_section >= 0)
    {
	display_section = new_section;
    }

    if (display_section > 0 && n < count)
    {
	track_timer.x = -1;

	q_in = spi_display_q_in;
	
	spi_display_q[q_in++] = display_section | 0xc0;

//	DDEBUG(("%d: ", display_section));

	for ( ; n < count; n++)
	{
	    spi_display_q[q_in++] = buf[n];
//	    DDEBUG(("%c", buf[n]));
	}

//	DDEBUG(("\n"));

	spi_display_q[q_in++] = display_section | 0x80;
	spi_display_q[q_in++] = display_section | 0x80;

	spi_display_q_in = q_in;
    }
    
    spin_unlock(&lp1sd_lock);
    return n;
}

int fpanel_read(struct file *filp, char *buf, size_t count, loff_t *offp)
{
    struct spi_s *s = &spi_info;
    unsigned short scancode;

    if (count != 2)
	return -EINVAL;
    
    if (!s->key_scancode && !s->encoder_change)
    {
	interruptible_sleep_on(&fpanel_input_wait);
	if (!s->key_scancode && !s->encoder_change)
	    return -EINTR;
    }

    if (s->key_scancode)
    {
	scancode = s->key_scancode;
	s->key_scancode = 0;
    }
    else
    {
	scancode = (s->encoder_change & 0xff) | 0x0100;
	s->encoder_change = 0;
    }
	
    copy_to_user(buf, &scancode, 2);
    return 2;
}

int fpanel_ioctl(struct inode *inode, struct file *filp, 
		 uint cmd, unsigned long arg)
{
    struct spi_s *s = &spi_info;
    unsigned short red;
    unsigned short green;
    unsigned short oldleds[2];

    if (cmd == CMD_SPI_DISPLAY_TRACK_TIMER)
    {
	struct track_timer_s *tinfo = (struct track_timer_s *) arg;

	spin_lock(&lp1sd_lock);

	track_timer = *tinfo;
	
	spin_unlock(&lp1sd_lock);

	return 0;
    }
    else if (cmd == CMD_SPI_DISPLAY_VOLUME_BARS)
    {
	struct volume_bar_s *vb = (struct volume_bar_s *) arg;

	spin_lock(&lp1sd_lock);

	volume_bars = *vb;
	send_volume_bars = 1;
	
	spin_unlock(&lp1sd_lock);

	return 0;
    }

    if (arg < 1 || arg > 8)
	return -EINVAL;

    green = 1 << (arg - 1);
    red = green << 8;

    oldleds[0] = s->led_image[0];
    oldleds[1] = s->led_image[1];

    switch (cmd)
    {
      case CMD_SPI_LED_OFF:
	s->led_image[0] &= ~(red | green);
	s->led_image[1] &= ~(red | green);
	break;
	  
      case CMD_SPI_LED_GREEN:
	s->led_image[0] &= ~red;
	s->led_image[1] &= ~red;
	s->led_image[0] |= green;
	s->led_image[1] |= green;
	break;
	
      case CMD_SPI_LED_RED:
	s->led_image[0] &= ~green;
	s->led_image[1] &= ~green;
	s->led_image[0] |= red;
	s->led_image[1] |= red;
	break;
	
      case CMD_SPI_LED_GREENFLASH:
	s->led_image[0] &= ~red;
	s->led_image[1] &= ~(red | green);
	s->led_image[0] |= green;
	break;
       
      case CMD_SPI_LED_REDFLASH:
	s->led_image[0] &= ~green;
	s->led_image[1] &= ~(red | green);
	s->led_image[0] |= red;
	break;

      case CMD_SPI_LED_REDGREENFLASH:
	s->led_image[0] &= ~green;
	s->led_image[1] &= ~red;
	s->led_image[0] |= red;
	s->led_image[1] |= green;
	break;

      default:
	return -EINVAL;
    }

    if (oldleds[0] != s->led_image[0] || oldleds[1] != s->led_image[1])
    {
	spi_transmit_leds = 1;
    }

    return 0;
}


int midi_open(struct inode *inode, struct file *filp)
{
    filp->f_op = &midi_fops;
    return 0;
}

int midi_close(struct inode *inode, struct file *filp)
{
    return 0;
}

int midi_read(struct file *filp, char *buf, size_t count, loff_t *offp)
{
    struct spi_s *s = &spi_info;
    int midi_buf_size = (sizeof(s->midi_in_q)/sizeof(s->midi_in_q[0]));
    int n;
    int n1;
    unsigned short q_in;

    /* If no data  */
    if (s->midi_in_q_in == s->midi_in_q_out)
    {
	if (filp->f_flags & O_NONBLOCK)
	    return 0;
	else
	{
	    interruptible_sleep_on(&midi_input_wait);
	    if (s->midi_in_q_in == s->midi_in_q_out)
	    {
		return -EINTR;
	    }
	}
    }

    q_in = s->midi_in_q_in;
    
    /* Determine how much to return to the user */
    n = (midi_buf_size + q_in - s->midi_in_q_out) % midi_buf_size;
    if (n > count)
	n = count;

    /* If q_in < q_out, then do the copy in two pieces */
    n1 = midi_buf_size - s->midi_in_q_out;
    if (n1 < n)
    {
	copy_to_user(buf, s->midi_in_q + s->midi_in_q_out, n1);
	copy_to_user(buf, s->midi_in_q, n - n1);
	s->midi_in_q_out = n - n1;
    }
    else
    {
	copy_to_user(buf, s->midi_in_q + s->midi_in_q_out, n1);
	s->midi_in_q_out += n;
    }

    return n;
}

int midi_write(struct file *filp, const char *buf, size_t count, loff_t *offp)
{
    struct spi_s *s = &spi_info;
    int midi_buf_size = (sizeof(s->midi_in_q)/sizeof(s->midi_in_q[0]));
    int n;
    int n1;

    n = s->midi_out_q_out - s->midi_out_q_in;
    if (n <= 0)
	n += midi_buf_size;

//    DDEBUG(("MIDI write: n = %d, count = %d\n", n, count));

    /* Enough room in queue? */
    if (n <= count)
	return 0;

    n1 = midi_buf_size - s->midi_out_q_in;
    
    if (count <= n1)
    {
	copy_from_user(s->midi_out_q + s->midi_out_q_in, buf, count);
	s->midi_out_q_in = (s->midi_out_q_in + count) % midi_buf_size;
    }
    else
    {
	copy_from_user(s->midi_out_q + s->midi_out_q_in, buf, n1);
	s->midi_out_q_in = 0;
	copy_from_user(s->midi_out_q + s->midi_out_q_in, buf + n1, count - n1);
	s->midi_out_q_in += count - n1;
    }

    return n;
}

int midi_ioctl(struct inode *inode, struct file *filp, 
	       uint cmd, unsigned long arg)
{
    unsigned long *lp;
	
    switch (cmd)
    {
      case CMD_SPI_SET_MIDI_CLOCK_HANDLER:
	lp = (unsigned long *) arg;

	midi_clock_handler = (midi_clock_handler_t) lp[0];
	DDEBUG(("midi_clock_handler = %lx\n", (long) midi_clock_handler));
	midi_clock_checker = (midi_clock_checker_t) lp[1];
	DDEBUG(("midi_clock_checker = %lx\n", (long) midi_clock_checker));
	break;

      default:
	return -EINVAL;
    }
    
    return 0;
}

unsigned char lp1sd_spi_io(unsigned char data_out)
{
    *pSPI_TDBR = data_out;

    // Wait for interface idle
    while (*pSPI_STAT & 0x0008)
	;
    while (!(*pSPI_STAT & 0x0001))
	;
    
    return *pSPI_RDBR;
}

unsigned char lp1sd_send_command(unsigned char *cmd)
{
    short i;
    unsigned char b;
    
    for (i = 0; i < 6; i++)
	b = lp1sd_spi_io(cmd[i]);
    
    return b;
}

int lp1sd_card_init(void)
{
    static unsigned char cmd0[6] = {0x40, 0x00,0x00,0x00,0x00,0x95};
    static unsigned char cmd1[6] = {0x41, 0x00,0x00,0x00,0x00,0xff};
    static unsigned char cmd41[6] = {0x40 | 41, 0x00,0x00,0x00,0x00,0xff};
    static unsigned char cmd55[6] = {0x40 | 55, 0x00,0x00,0x00,0x00,0xff};
    short i;
    short j;
    unsigned char b;
    
    spin_lock(&lp1sd_lock);

    // Configure PF signals
    *pFIO_DIR |= 0x001c;	// PF2 and PF3 and PF4 are outputs
    *pFIO_FLAG_S = 0x001c;	// PF2 and PF3 and PF4 are idle high

    *pFIO_DIR &= ~0x0060;	// PF5 and PF6 are inputs
    *pFIO_MASKA_C = 0x0060;	// PF5 and PF6 don't trigger interrupts
    *pFIO_MASKB_C = 0x0060;
    *pFIO_POLAR &= ~0x0060;
    *pFIO_EDGE &= ~0x0060;
    *pFIO_INEN |= 0x0060;	// enable PF5 and PF6 input buffers

    // Configure SPI port
    *pSPI_BAUD = BAUD_SLOW;
    *pSPI_CTL = 0x5001;
    *pSPI_FLG = 0x0800;

    // Turn off card power -- currently not possible
    DDEBUG(("lp1sd: initializing\n"));
    
    // Send 100 clock cycles to card to initialize it.
    for (i = 0; i < 50; i++)
	lp1sd_spi_io(0xff);

    // Select card and send CMD0 (Go idle state)
    *pFIO_FLAG_C = 0x0004;	// Enable select
    lp1sd_send_command(cmd0);

    b = 0xff;
    for (i = 0; i < 8 && b == 0xff; i++)
	b = lp1sd_spi_io(0xff);

    *pFIO_FLAG_S = 0x0004;	// Disable select
    lp1sd_spi_io(0xff);

    if (b != 0x01)
    {
	// CMD0 failed
	DDEBUG(("lp1sd: CMD0 failure. Continuing.\n"));
    }

    // Wait for card to initialize
    for (j = 0; j < 10000; j++)
    {
	*pFIO_FLAG_C = 0x0004;	// Enable select

	lp1sd_send_command(cmd1);
	b = 0xff;
	for (i = 0; i < 8 && b == 0xff; i++)
	    b = lp1sd_spi_io(0xff);
	
	*pFIO_FLAG_S = 0x0004;	// Disable select
	lp1sd_spi_io(0xff);

	if (b == 0x00)
	    break;
    }

    if (b != 0x00)
    {
	spin_unlock(&lp1sd_lock);
	return 2;
    }

    // Verify that this is indeed an SD card and not an MMC card
    *pFIO_FLAG_C = 0x0004;	// Enable select
    lp1sd_send_command(cmd55);	// Prefix for app specific commands
    b = 0xff;
    for (i = 0; i < 8 && b == 0xff; i++)
	b = lp1sd_spi_io(0xff);
    
    *pFIO_FLAG_S = 0x0004;	// Disable select
    lp1sd_spi_io(0xff);

    for (j = 0; j < 50; j++)
    {
	*pFIO_FLAG_C = 0x0004;	// Enable select

	lp1sd_send_command(cmd41);
	b = 0xff;
	for (i = 0; i < 8 && b == 0xff; i++)
	    b = lp1sd_spi_io(0xff);
	
	*pFIO_FLAG_S = 0x0004;	// Disable select
	lp1sd_spi_io(0xff);

	if (b == 0x00)
	    break;
    }

    if (b != 0x00)
    {
	spin_unlock(&lp1sd_lock);
	return 3;
    }

    spin_unlock(&lp1sd_lock);
    return 0;
}

int lp1sd_get_csd(void)
{
    static unsigned char cmd9[6] = {0x40 | 9, 0x00,0x00,0x00,0x00,0xff};
    static unsigned char csd[16];
    short i;

    unsigned int c_size;
    unsigned int c_size_mult;
    unsigned int mult;
    unsigned int read_bl_len;
    unsigned int blocknr = 0;
    unsigned int block_len = 0;
    unsigned int size = 0;

    spin_lock(&lp1sd_lock);

    for (i = 0; i < 6; i++)
	spi_command_buffer[i] = cmd9[i];
    spi_command_length = 6;
    spi_data_length = 18;
    spi_data_direction = SPI_DATADIRECTION_RECV;
    spi_command_response = 0xff;
    
    spi_buffer_idx = 0;
    spi_command_mode = SPI_MODE_SEND_COMMAND;

    while (spi_command_mode != SPI_MODE_IDLE)
	;

    if (spi_command_response != 0x00)		// failure?
    {
	spin_unlock(&lp1sd_lock);
	return 4;
    }
    
    // Collect CSD
    for (i = 0; i < 16; i++)
	csd[i] = spi_command_data[i];

    c_size = csd[8] + csd[7] * 256 + (csd[6] & 0x03) * 256 * 256;
    c_size >>= 6;
    c_size_mult = csd[10] + (csd[9] & 0x03) * 256;
    c_size_mult >>= 7;
    read_bl_len = csd[5] & 0x0f;
    mult = 1;
    mult <<= c_size_mult + 2;
    blocknr = (c_size + 1) * mult;
    block_len = 1;
    block_len <<= read_bl_len;
    size = block_len * blocknr;
    size >>= 10;

    lp1sd_blk_sizes[0] = size;
    lp1sd_blksize_sizes[0] = 1024;
    lp1sd_hardsect_sizes[0] = block_len;
    lp1sd_sectors = blocknr;

    spin_unlock(&lp1sd_lock);

    DDEBUG(("lp1sd: CSD: %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x\n",
	   csd[0], csd[1], csd[2], csd[3], 
	   csd[4], csd[5], csd[6], csd[7], 
	   csd[8], csd[9], csd[10], csd[11], 
	   csd[12], csd[13], csd[14], csd[15]));
    DDEBUG(("lp1sd: size %u, block length %u, blocks %u\n", 
	   size, block_len, blocknr));

    return 0;
}

void lp1sd_prepare_read_sector(int sector)
{
    static unsigned char cmd17[6] = {0x40 | 17, 0x00,0x00,0x00,0x00,0xff};
    unsigned long offset;
    short i;

    offset = (unsigned long) sector * 512;

    // Fill in command block
    cmd17[1] = (unsigned char) (offset >> 24);
    cmd17[2] = (unsigned char) (offset >> 16);
    cmd17[3] = (unsigned char) (offset >> 8);
    cmd17[4] = (unsigned char) offset;

    for (i = 0; i < 6; i++)
	lp1sd_command_buffer[i] = cmd17[i];
    lp1sd_command_length = 6;
    lp1sd_data_length = 514;
    lp1sd_data_direction = SPI_DATADIRECTION_RECV;
    lp1sd_command_response = 0xff;
}

int lp1sd_read_sector(struct request *req, int sector, unsigned char *buffer)
{
    lp1sd_request_done = 0;
    lp1sd_current_sector_n = 0;
    lp1sd_prepare_read_sector(sector);
    lp1sd_current_req = req;

    return 1;
}

void lp1sd_prepare_write_sector(struct request *req, int sector)
{
    static unsigned char cmd24[6] = {0x40 | 24, 0x00,0x00,0x00,0x00,0xff};
    unsigned long offset;
    unsigned char *p;
    int i;

    offset = (unsigned long) sector * 512;

    // Fill in command block
    cmd24[1] = (unsigned char) (offset >> 24);
    cmd24[2] = (unsigned char) (offset >> 16);
    cmd24[3] = (unsigned char) (offset >> 8);
    cmd24[4] = (unsigned char) offset;
    
    // Send WRITE SINGLE BLOCK command
    for (i = 0; i < 6; i++)
	lp1sd_command_buffer[i] = cmd24[i];
    lp1sd_command_length = 6;

    p = (unsigned char *) req->buffer;
    p += (lp1sd_current_sector_n * 512);
    
    memcpy(lp1sd_command_data, p, 512);
    lp1sd_command_data[512] = 0xff;
    lp1sd_command_data[513] = 0xff;
    lp1sd_data_length = 514;

    lp1sd_data_direction = SPI_DATADIRECTION_SEND;
    lp1sd_command_response = 0xff;
}

int lp1sd_write_sector(struct request *req, int sector, unsigned char *buffer)
{
    lp1sd_request_done = 0;
    lp1sd_current_sector_n = 0;
    lp1sd_prepare_write_sector(req, sector);
    lp1sd_current_req = req;

    return 1;
}

int lp1sd_init(void)
{
    int rv;
    
    // Initialize the card
    rv = lp1sd_card_init();
    if (rv != 0)
	return rv;

    // Start the interrupt process
    *pSPI_BAUD = BAUD_PIC;
    *pSPI_CTL = 0x5000;
    *pSPI_TDBR = 0xff;

    if (request_irq(IRQ_SPI, &spi_handler, SA_INTERRUPT, 
		    "SPI", (void *) &spi_stalled))
    {
	DDEBUG(("Failed to allocate SPI interrupt\n"));
	return -ENODEV;
    }

    *pSPI_RDBR;
    
    // Read the geometry from the card
    rv = lp1sd_get_csd();
    if (rv != 0)
	return rv;

    return 0;
}

void lp1sd_exit(void)
{
    lp1sd_sectors = 0;

    if (lp1sd_gd)
    {
	del_gendisk(lp1sd_gd);
	lp1sd_gd = NULL;
    }
}

int lp1sd_open(struct inode *inode, struct file *filp)
{
    return 0;
}

int lp1sd_release(struct inode *inode, struct file *filp)
{
    return 0;
}

int lp1sd_ioctl(struct inode *inode, struct file *filp, 
		unsigned int cmd, unsigned long arg)
{
    struct hd_geometry geo;
    
    switch (cmd)
    {
      case HDIO_GETGEO:
	if (!arg)
	    return -EINVAL;
	geo.sectors = lp1sd_sectors;
	geo.heads = 0;
	geo.cylinders = 0;
	geo.start = 0;
	if (copy_to_user((void __user *) arg, &geo, sizeof(geo)))
	    return -EFAULT;
	return 0;
	
      case BLKGETSIZE:
	if (copy_to_user((void __user *) arg, &lp1sd_sectors, sizeof(long)))
	    return -EFAULT;
	return 0;
    }
    
    return -EINVAL;
}

void lp1sd_request(request_queue_t *q)
{
    struct request *req;
    int success;

    while ((req = elv_next_request(q)) != NULL)
    {
	success = 0;
	
	if (! blk_fs_request(req))
	{
	    DDEBUG(("lp1sd: skipping non-fs request\n"));
	    end_request(req, 0);
	    continue;
	}

	if (rq_data_dir(req))
	{
	    // WRITE
	    success = lp1sd_write_sector(req, req->sector, req->buffer);
	    if (!success)
		end_request(req, 0);
	}
	else
	{
	    // READ
	    success = lp1sd_read_sector(req, req->sector, req->buffer);
	    if (!success)
		end_request(req, 0);
	}

	// Wait for previous request to complete
	spin_unlock_irq(q->queue_lock);
	
	while (!lp1sd_request_done)
	    ;

	spin_lock_irq(q->queue_lock);
    }
}

int __init lp1sd_init_driver(void)
{
    int rc;

    spi_info.led_image[0] = 0;
    spi_info.led_image[1] = 0;

    spi_major = register_chrdev(SPI_MAJOR, "spi", &spi_fops);
    
    spin_lock(&lp1sd_lock);

    rc = lp1sd_init();
    if (rc != 0) 
	DDEBUG(("lp1sd: error in lp1sd_init (%d)\n", rc));
    else
	DDEBUG(("lp1sd: lp1sd_init completed\n"));

    lp1sd_major = register_blkdev(0, "lp1sd");
    if (lp1sd_major <= 0)
    {
	DDEBUG(("lp1sd: unable to get major number\n"));
	return -EBUSY;
    }

    lp1sd_queue = blk_init_queue(lp1sd_request, &lp1sd_lock);

    // Create the disk for this card

    lp1sd_gd = alloc_disk(4);
    if (lp1sd_gd == NULL)
    {
	DDEBUG(("lp1sd: alloc disk failure\n"));
	return 10;
    }
    lp1sd_gd->major = lp1sd_major;
    lp1sd_gd->first_minor = 0;
    lp1sd_gd->minors = 4;
    lp1sd_gd->fops = &lp1sd_bdops;
    lp1sd_gd->private_data = NULL;
    lp1sd_gd->queue = lp1sd_queue;
    snprintf(lp1sd_gd->disk_name, 32, "lp1sda");
    set_capacity(lp1sd_gd, lp1sd_sectors * (lp1sd_hardsect_sizes[0] / 512));
    add_disk(lp1sd_gd);

    spin_unlock(&lp1sd_lock);

    return 0;
}

void __exit lp1sd_exit_driver(void)
{
    unregister_chrdev(spi_major, "spi");
}

struct block_device_operations lp1sd_bdops = 
{
    .open = lp1sd_open,
    .release = lp1sd_release,
    .ioctl = lp1sd_ioctl,
    .owner = THIS_MODULE
};

module_init(lp1sd_init_driver);
module_exit(lp1sd_exit_driver);

MODULE_LICENSE("Looperlative Audio Propietary");
MODULE_AUTHOR("Robert Amstadt");
MODULE_DESCRIPTION("LP1 SD module");
