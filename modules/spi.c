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

#include "spi.h"
#include "bitreverse.h"

/*
 * PFn usage:
 *
 * 0: 1 Ethernet CS, 0 Flash CS
 * 1: unused
 * 2: UART selector
 * 3: slave select
 * 4: MIDI irq input on 0301 boards, debug output on STAMP board
 * 5: debug output on STAMP
 * 6: unused
 * 7: unused
 */

#define SSYNC __builtin_bfin_ssync()

static int spi_send(struct spi_s *s, unsigned short data);

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

#define SPI_MAJOR		252
#undef TEST_MIDI_INPUT
#undef DEBUG_COMM_SPEED

struct encoder_state_s
{
    short current_state;
    short encoder_value;
    short go_to_state;
    short encoder_count;
};

#define ENC_STATE_START		0
#define ENC_STATE_CCW1		1
#define ENC_STATE_CCW2		2
#define ENC_STATE_CCW3		3
#define ENC_STATE_CCW4		4
#define ENC_STATE_CW1		5
#define ENC_STATE_CW2		6
#define ENC_STATE_CW3		7
#define ENC_STATE_CW4		8

static struct encoder_state_s encoder_machine[] =
{
    { ENC_STATE_START,	0x0400, ENC_STATE_CCW1,	0 },
    { ENC_STATE_START,	0x0200, ENC_STATE_CW1,	0 },

    { ENC_STATE_CCW1,	0x0400, ENC_STATE_CCW1,	0 },
    { ENC_STATE_CCW1,	0x0000, ENC_STATE_CCW2,	0 },
    { ENC_STATE_CCW1,	0x0c00, ENC_STATE_START, 0 },

    { ENC_STATE_CCW2,	0x0000, ENC_STATE_CCW2,	0 },
    { ENC_STATE_CCW2,	0x0200, ENC_STATE_CCW3,	0 },
    { ENC_STATE_CCW2,	0x0c00, ENC_STATE_START, 0 },

    { ENC_STATE_CCW3,	0x0200, ENC_STATE_CCW3,	0 },
    { ENC_STATE_CCW3,	0x0600, ENC_STATE_START, -1 },
    { ENC_STATE_CCW3,	0x0c00, ENC_STATE_START, 0 },

    { ENC_STATE_CW1,	0x0200, ENC_STATE_CW1,	0 },
    { ENC_STATE_CW1,	0x0000, ENC_STATE_CW2,	0 },
    { ENC_STATE_CW1,	0x0c00, ENC_STATE_START, 0 },

    { ENC_STATE_CW2,	0x0000, ENC_STATE_CW2,	0 },
    { ENC_STATE_CW2,	0x0400, ENC_STATE_CW3,	0 },
    { ENC_STATE_CW2,	0x0c00, ENC_STATE_START, 0 },

    { ENC_STATE_CW3,	0x0400, ENC_STATE_CW3,	0 },
    { ENC_STATE_CW3,	0x0600, ENC_STATE_START, 1 },
    { ENC_STATE_CW2,	0x0c00, ENC_STATE_START, 0 },

    { -1,		0, 	ENC_STATE_START, 0 },
};

static unsigned short spi_green_leds[8] =
{
    0x4000, 0x1000, 0x0400, 0x0100, 0x0040, 0x0010, 0x0004, 0x0001
};

static struct timer_list led_timer;

static spinlock_t spi_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t spi_irq_lock = SPIN_LOCK_UNLOCKED;
static int spi_major = -1;
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
static unsigned short spi_watchdog = 0;
DECLARE_WAIT_QUEUE_HEAD (midi_input_wait);
DECLARE_WAIT_QUEUE_HEAD (fpanel_input_wait);
DECLARE_WAIT_QUEUE_HEAD (fpanel_output_wait);

typedef int (*midi_clock_handler_t)(unsigned long arg);
typedef short (*midi_clock_checker_t)(void);
static midi_clock_handler_t midi_clock_handler = NULL;
static midi_clock_checker_t midi_clock_checker = NULL;


static void led_timeout(unsigned long data)
{
    struct spi_s *s = (struct spi_s *) data;
    unsigned short n;
    
    led_timer.expires = jiffies + HZ / 4;
    add_timer(&led_timer);

    s->current_image ^= 1;

    spi_send(s, ((s->led_image[s->current_image] & 0xff) << 8) | 0x20);
    spi_send(s, (s->led_image[s->current_image] & 0xff00) | 0x80);

    /* Check and make sure that SPI is still running */
    if (!spi_watchdog)
    {
	n = *pSPI_RDBR;
    }

    spi_watchdog = 0;

#ifdef TEST_MIDI_INPUT
    while (s->midi_in_q_in != s->midi_in_q_out)
    {
	printk("MIDI: %x\n", s->midi_in_q[s->midi_in_q_out]);

	s->midi_in_q_out++;
	if (s->midi_in_q_out >= (sizeof(s->midi_in_q)/sizeof(s->midi_in_q[0])))
	    s->midi_in_q_out = 0;
    }

    if (s->key_scancode)
    {
	printk("Keypress %x\n", s->key_scancode);
	s->key_scancode = 0;
    }

    if (s->encoder_change)
    {
	printk("Encoder %d\n", s->encoder_change);
	s->encoder_change = 0;
    }
#endif
}

static irqreturn_t spi_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    struct spi_s *s = (struct spi_s *) dev_id;
    unsigned short n;
    unsigned short send_scan_code;
    unsigned short in_data;
    int current_mode = s->comm_mode;
    int i;
    unsigned long flags;
    unsigned short mcc;

    spin_lock_irqsave(&spi_irq_lock, flags);

    spi_watchdog = 1;

    n = *pSPI_STAT;
    in_data = *pSPI_SHADOW;

    /* If transfer in progress then don't do anything */
    if (!(n & 0x0001))
    {
	spin_unlock_irqrestore(&spi_irq_lock, flags);
	return IRQ_HANDLED;
    }

    /* Increment interrupt count.  Allows us to determine comm speed */
    s->interrupts++;

    /*
      Scan keyboard.  Cycle through the columns and look for one of the rows
      to indicate a key down.  Once that happens, remember key pressed and
      debounce.
    */
    send_scan_code = 0;
    
    if (s->key_scan_delay > 0)
	s->key_scan_delay--;
    else
    {
	switch (s->key_state)
	{
	  case SPI_KEY_UP:
	    /* Any key on this column pressed? */
	    n = ~*pFIO_FLAG_D;
	    if (n & 0xf800)
	    {
		s->key_scancode = (n & 0xf800) | (1 << s->key_column);
		s->key_state = SPI_KEY_DEBOUNCEDOWN;
		s->key_scan_delay = SPI_KEY_DEBOUNCEDELAY;
		wake_up_interruptible(&fpanel_input_wait);
	    }
	    else
	    {
		s->key_column++;
		if (s->key_column >= 8)
		    s->key_column = 0;
		
		send_scan_code = ((0xfeff << s->key_column) & 0xff00) | 0x40;
		s->key_scan_delay = SPI_KEY_SCANDELAY;
	    }
	    break;
	  case SPI_KEY_DOWN:
	    /* Key released yet? */
	    n = ~*pFIO_FLAG_D;
	    if (!(n & 0xf800))
	    {
		s->key_state = SPI_KEY_DEBOUNCEUP;
		s->key_scan_delay = SPI_KEY_DEBOUNCEDELAY;
	    }
	    else
		s->key_scan_delay = SPI_KEY_SCANDELAY;
	    break;
	  case SPI_KEY_DEBOUNCEDOWN:
	    s->key_state = SPI_KEY_DOWN;
	    s->key_scan_delay = SPI_KEY_SCANDELAY;
	    break;
	  case SPI_KEY_DEBOUNCEUP:
	    s->key_state = SPI_KEY_UP;
	    s->key_scan_delay = SPI_KEY_SCANDELAY;
	    break;
	}
    }

    /*
      Scan code to send?
    */
    if (send_scan_code)
    {
	*pSPI_FLG = 0x0c04;
	*pSPI_TDBR = send_scan_code;
	s->comm_mode = SPI_COMM_MODE_PF2;
    }
    /* 
       MIDI clock out has second highest priority.
    */
    else if (current_mode == SPI_COMM_MODE_MIDI_RX &&
	     midi_clock_checker != NULL &&
	     (in_data & 0x0002) &&
	     (mcc = (*midi_clock_checker)()) != 0)
    {
	*pSPI_FLG = 0x0c08;

	if (mcc == 2)
	    *pSPI_TDBR = 0x5f01;
	else if (mcc == 3)
	    *pSPI_TDBR = 0x3f01;
	else
	    *pSPI_TDBR = 0x1f01;

	s->comm_mode = SPI_COMM_MODE_MIDI_TX;
    }
    /* 
       If something is in the send queue and the last cycle was a MIDI cycle,
       then send the transaction.
    */
    else if (current_mode == SPI_COMM_MODE_MIDI_RX && 
	     !(in_data & 0x0001) &&
	     s->spi_send_q_in != s->spi_send_q_out)
    {
	*pSPI_FLG = 0x0c04;

	/* If display is busy, then send dummy transaction */
	if (s->display_busy)
	{
	    *pSPI_TDBR = 0x00a0;
	}
	/* Otherwise send actual queued transaction */
	else
	{
	    *pSPI_TDBR = s->spi_send_q[s->spi_send_q_out];

	    n = s->spi_send_q_out + 1;
	    if (n >= (sizeof(s->spi_send_q) / sizeof(s->spi_send_q[0])))
		n = 0;
	    s->spi_send_q_out = n;

	    wake_up_interruptible(&fpanel_output_wait);
	    s->display_busy = 1;
	}
	
	s->comm_mode = SPI_COMM_MODE_PF2;
    }
    /*
      Otherwise, if previous transaction was to a PF2 device, then need to
      change SPI settings back to MIDI.
    */
    else if (current_mode == SPI_COMM_MODE_PF2 ||
	     current_mode == SPI_COMM_MODE_MIDI_TX)
    {
	*pSPI_FLG = 0x0c08;

	if (current_mode == SPI_COMM_MODE_MIDI_RX &&
	    (in_data & 0x0002) &&
	    s->midi_out_q_in != s->midi_out_q_out)
	{
	    n = s->midi_out_q[s->midi_out_q_out++];
	    if (s->midi_out_q_out >= 256)
		s->midi_out_q_out = 0;

	    n = (unsigned short) BitReverseTable256[n] << 8;
	    *pSPI_TDBR = 0x0001 | n;
	    s->comm_mode = SPI_COMM_MODE_MIDI_TX;
	}
	else
	{
	    *pSPI_TDBR = 0x0000;
	    s->comm_mode = SPI_COMM_MODE_MIDI_RX;
	}
    }

    /* Get MIDI data from SPI, if data valid, queue it for application */
    n = *pSPI_RDBR;	/* Side effect: initiates next transaction */

    if ((current_mode == SPI_COMM_MODE_MIDI_RX ||
	 current_mode == SPI_COMM_MODE_MIDI_TX) && 
	(in_data & 0x00fd) == 0x0001)
    {
	in_data = BitReverseTable256[(unsigned char) (in_data >> 8)];
	
	if ((unsigned char) in_data == 0xf8 && midi_clock_handler != NULL)
	{
	    (*midi_clock_handler)(0);
	}
	else
	{
	    s->midi_in_q[s->midi_in_q_in] = (unsigned char) in_data;

	    n = s->midi_in_q_in + 1;
	    if (n >= (sizeof(s->midi_in_q)/sizeof(s->midi_in_q[0])))
		n = 0;
	    if (n != s->midi_in_q_out)
	    {
		s->midi_in_q_in = n;
		wake_up_interruptible(&midi_input_wait);
	    }
	}
    }
    else if (current_mode == SPI_COMM_MODE_PF2)
    {
	s->display_busy = n;
    }

    /* Process encoder */
    n = *pFIO_FLAG_D & 0x0600;

    if (n == s->encoder_lastval)
    {
	if (s->encoder_debounce < 100)
	    s->encoder_debounce++;
    }
    else
    {
	s->encoder_debounce = 0;
	s->encoder_lastval = n;
    }
    
    if (s->encoder_debounce >= 20)
    {
	for (i = 0; encoder_machine[i].current_state != -1; i++)
	{
	    if (encoder_machine[i].current_state == s->encoder_state &&
		encoder_machine[i].encoder_value == n)
	    {
		s->encoder_state = encoder_machine[i].go_to_state;
		s->encoder_change += encoder_machine[i].encoder_count;
		break;
	    }
	}

	if (s->encoder_change != 0)
	    wake_up_interruptible(&fpanel_input_wait);
    }

    spin_unlock_irqrestore(&spi_irq_lock, flags);

    return IRQ_HANDLED;
}

static unsigned short midi_xfer_immediate(unsigned short data)
{
    *pSPI_TDBR = data;

    while (*pSPI_STAT & 0x0008)
	SSYNC;
    while (!(*pSPI_STAT & 0x0001))
	SSYNC;

    return *pSPI_RDBR;
}

static void midi_configure(struct spi_s *s)
{
    /* Initialize the SPI to communicate with the MIDI interface */
    SSYNC;
    *pSPI_BAUD = 200;
    *pSPI_CTL = 0x5301;
    *pSPI_FLG = 0x0c08;
    SSYNC;
    
    /* Configure the UART for correct baud rate */
    printk("UART write config: %x\n", midi_xfer_immediate(0xc003));
    printk("UART read config: %x\n", midi_xfer_immediate(0x0002));
}

static int spi_send(struct spi_s *s, unsigned short data)
{
    unsigned long flags;
    short n;
    int rv;

    spin_lock_irqsave(&spi_lock, flags);
    
    s->spi_send_q[s->spi_send_q_in] = data;
    n = s->spi_send_q_in + 1;
    if (n >= (sizeof(s->spi_send_q)/sizeof(s->spi_send_q[0])))
	n = 0;
    if (n != s->spi_send_q_out)
    {
	s->spi_send_q_in = n;
	rv = 1;
    }
    else
	rv = 0;

    spin_unlock_irqrestore(&spi_lock, flags);

    return rv;
}

int init_module(void)
{
    volatile unsigned short n;
    
    spi_major = register_chrdev(SPI_MAJOR, "spi", &spi_fops);
/*    printk("Registered SPI device as %d\n", spi_major); */

    /* Initialize device structure */
    spi_info.midi_in_q_in = 0;
    spi_info.midi_in_q_out = 0;

    spi_info.midi_out_q_in = 0;
    spi_info.midi_out_q_out = 0;

    spi_info.spi_send_q_in = 0;
    spi_info.spi_send_q_out = 0;

    spi_info.interrupts = 0;
    spi_info.comm_mode = SPI_COMM_MODE_MIDI_RX;

    spi_info.led_image[0] = 0;
    spi_info.led_image[1] = 0;
    spi_info.current_image = 0;

    spi_info.key_scan_delay = 30;
    spi_info.key_column = 0;
    spi_info.key_state = SPI_KEY_UP;
    spi_info.key_scancode = 0;

    spi_info.encoder_state = 0;
    spi_info.encoder_change = 0;
    spi_info.encoder_lastval = 0;
    spi_info.encoder_debounce = 0;

    /* Set default inactive values for PF2 and PF3 */
    SSYNC;
    *pFIO_DIR |= 0x001c;
    *pFIO_FLAG_S = 0x001c;

    /* Enable PF key inputs */
    *pFIO_DIR &= ~0xf800;
    *pFIO_MASKA_C = 0xf800;	/* keys don't trigger interrupt */
    *pFIO_MASKB_C = 0xf800;	/* keys don't trigger interrupt */
    *pFIO_POLAR &= ~0xf800;
    *pFIO_EDGE &= ~0xf800;
    *pFIO_INEN |= 0xf800;	/* Enable input buffers */
    SSYNC;

    /* Configure the MIDI interface */
    midi_configure(&spi_info);

    /* Configure the SPI port to poll MIDI interface and start polling */
    SSYNC;
    *pSPI_BAUD = 125;
    *pSPI_FLG = 0x0c08;
    *pSPI_CTL = 0x5304;
    SSYNC;
    n = *pSPI_RDBR;		/* Start polling using a read from data reg */
    SSYNC;

    /* Install interrupt handler for SPI */
    if (request_irq(IRQ_SPI, &spi_handler, SA_INTERRUPT, "SPI", &spi_info))
    {
/*	printk("Failed to allocate SPI interrupt, %d\n", IRQ_SPI); */
	return -ENODEV;
    }

    /* Install led timer handler */
    init_timer(&led_timer);
    led_timer.function = led_timeout;
    led_timer.data = (unsigned long) &spi_info;
    led_timer.expires = jiffies + HZ / 4;
    add_timer(&led_timer);

    /* Turn off all key columns */
    spi_send(&spi_info, 0xff40);

    /* Initialize LEDs */
    spi_send(&spi_info, 0x0020);
    spi_send(&spi_info, 0x0080);

    /*
      Initialize rotary encoder inputs.
    */
    SSYNC;
    *pFIO_MASKA_C = 0x0600;	/* no interrupts */
    *pFIO_MASKB_C = 0x0600;	/* no interrupts */

    *pFIO_DIR &= ~0x0600;	/* PF lines for encoder are inputs */
    *pFIO_INEN |= 0x0600;	/* Enable input buffers */
    *pFIO_EDGE &= ~0x0600;	/* Level triggered */
    *pFIO_POLAR &= ~0x0600;	/* Active high polarity */
    SSYNC;

    return 0;
}

void cleanup_module(void)
{
    SSYNC;
    *pSPI_CTL = 0;
    SSYNC;

    /* Disconnect from interrupt */
    free_irq(IRQ_SPI, &spi_info);
    
    /* Delete system timer */
    del_timer(&led_timer);

    unregister_chrdev(spi_major, "spi");
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
    struct spi_s *s = &spi_info;
    unsigned char temp_buf[100];
    int n;
    int nqueue;
    int queuelength;
    unsigned short data;
    
    if (count > 100)
	return -EINVAL;

    /* Wait for enough space in the queue */
    queuelength = sizeof(s->spi_send_q) / sizeof(s->spi_send_q[0]);

    nqueue = queuelength + s->spi_send_q_in - s->spi_send_q_out;
    nqueue %= queuelength;
    while (queuelength - nqueue <= count)
    {
	interruptible_sleep_on(&fpanel_output_wait);

	nqueue = queuelength + s->spi_send_q_in - s->spi_send_q_out;
	nqueue %= queuelength;
    }

    /* Queue data to display */
    copy_from_user(temp_buf, buf, count);
    for (n = 0; n < count; n++)
    {
	data = ((unsigned short) temp_buf[n] << 8) | 0xc0;
	
	if (spi_send(&spi_info, data) == 0)
	    break;
    }
    
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

    if (arg < 1 || arg > 8)
	return -EINVAL;

    green = spi_green_leds[arg-1];
    red = green << 1;

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

//    printk("MIDI write: n = %d, count = %d\n", n, count);

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
	printk("midi_clock_handler = %lx\n", (long) midi_clock_handler);
	midi_clock_checker = (midi_clock_checker_t) lp[1];
	printk("midi_clock_checker = %lx\n", (long) midi_clock_checker);
	break;

      default:
	return -EINVAL;
    }
    
    return 0;
}

MODULE_LICENSE("Looperlative Audio Products Proprietary");
MODULE_AUTHOR("Robert Amstadt <bob@looperlative.com>");
MODULE_DESCRIPTION("Looperlative LP1 looping controller");
