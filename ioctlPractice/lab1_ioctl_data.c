/* ******************************** 
 *
 * The code in this driver is primarily from an excersize from the book
 * Linux Device Drivers: a guide with excersizes and 
 * a led driver from sysprogs.com. By merging the two 
 * drivers and making slight modifications, the driver has one hw line act as 
 * a clock and the other line print SOS through an LED in morse code. 
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/module.h>
#include <asm/io.h>
#include <mach/platform.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/err.h>

#include "lab_miscdev.h"

#define MYIOC_TYPE 'k'
#define INTRA_LETTER_SPACE 'l'
#define INTRA_WORD_SPACE 'w'
#define INTER_LETTER_SPACE 's'
#define DASH '-'
#define DOT '.'


struct GpioRegisters
{
	uint32_t GPFSEL[6];
	uint32_t Reserved1;
	uint32_t GPSET[2];
	uint32_t Reserved2;
	uint32_t GPCLR[2];
};

struct GpioRegisters * s_pGpioRegisters;

struct my_data {
	char s[256];
};

static struct my_data my_data = {
	.s = "original string",
};

static struct timer_list s_BlinkTimer;
static struct timer_list clockTimer;
static const int clockPeriod = 1;
static const int ClockGpioPin = 23;
static const int LedGpioPin = 18;
static int messageIdx = 0;
static const int dotLength = 10;
static const int dashLength = 30;
static const int interLetterLength = 10;
static const int intraLetterLength = 30;
static const int intraWordLength = 70;

static void SetGPIOFunction(int GPIO, int functionCode)
{
	int registerIndex = GPIO / 10;
	int bit = (GPIO % 10) * 3;

	unsigned oldValue = s_pGpioRegisters->GPFSEL[registerIndex];
	unsigned mask = 0b111 << bit;
	printk("changing function of GPIO%d from %x to %x\n", GPIO, \
		(oldValue >> bit) & 0b111, functionCode);
	s_pGpioRegisters->GPFSEL[registerIndex] = (oldValue & ~mask)\
		| ((functionCode << bit) & mask);
}

static void SetGPIOOutputValue(int GPIO, bool outputValue)
{
	if(outputValue)
		s_pGpioRegisters->GPSET[GPIO / 32] = (1 << (GPIO % 32));
	else
		s_pGpioRegisters->GPCLR[GPIO / 32] = (1 << (GPIO % 32));
}

static void ClockTimerHandler(unsigned long unused)
{
	static bool on = false;
	on = !on;
	SetGPIOOutputValue(ClockGpioPin, on);
	mod_timer(&clockTimer, jiffies + msecs_to_jiffies(clockPeriod));
}

static void BlinkTimerHandler(unsigned long unused)
{
	pr_info("my_data.s[%d] = %c\n",messageIdx, my_data.s[messageIdx]);
	if(my_data.s[messageIdx] == '\0' || messageIdx >= 256)
	{
		SetGPIOOutputValue(LedGpioPin, false);
		return;
	}
	switch(my_data.s[messageIdx++])
	{
		case(INTRA_LETTER_SPACE):
		{
			SetGPIOOutputValue(LedGpioPin, false);
			mod_timer(&s_BlinkTimer, jiffies +\
				msecs_to_jiffies(intraLetterLength));
			break;
		}
		case(INTRA_WORD_SPACE):
		{
			SetGPIOOutputValue(LedGpioPin, false);
			mod_timer(&s_BlinkTimer, jiffies + \
				 msecs_to_jiffies(intraWordLength));
			break;
		}
		case(INTER_LETTER_SPACE):
		{
			SetGPIOOutputValue(LedGpioPin, false);
			mod_timer(&s_BlinkTimer, jiffies +\
				msecs_to_jiffies(interLetterLength));
			break;
		}
		case(DASH):
		{
			SetGPIOOutputValue(LedGpioPin, true);
			mod_timer(&s_BlinkTimer, jiffies +\
				msecs_to_jiffies(dashLength));
			break;
		}
		case(DOT):
		{
			SetGPIOOutputValue(LedGpioPin, true);
			mod_timer(&s_BlinkTimer, jiffies +\
				msecs_to_jiffies(dotLength));
			break;
		}
		default:
		{
			pr_err("invalid character sent from ioctl");
			SetGPIOOutputValue(LedGpioPin, false);
			break;
		}
	}
}

static long
mycdrv_unlocked_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int size;
	int rc;
	int direction;
	void __user *ioargp = (void __user *)arg;

	pr_info( " entering mycdrv ioctl call\n");
	
	if (_IOC_TYPE(cmd) != MYIOC_TYPE) {
		pr_info(" got invalid case, CMD=%d\n", cmd);
		return -EINVAL;
	}

	direction = _IOC_DIR(cmd);
	size = _IOC_SIZE(cmd);

	switch (direction) {

	case _IOC_WRITE:
		pr_info(
		     " reading = %d bytes from user-space and writing to device\n",
		     size);
		rc = copy_from_user(&my_data, ioargp, size);
		pr_info(
		     "    my_data.s = %s\n",
		     my_data.s);
		
		messageIdx = 0;
		ClockTimerHandler(0);
		BlinkTimerHandler(0);
		return rc;
		break;

	default:
		pr_info(" got invalid case, CMD=%d\n", cmd);
		return -EINVAL;
	}
}


static int __init LedBlinkModule_init(void)
{
	ramdisk = kmalloc(ramdisk_size, GFP_KERNEL);
	if(misc_register(&my_misc_device))
	{
		pr_err("Couldn't register device misc, %d.\n", \
			my_misc_device.minor);
		kfree(ramdisk);
		return -EBUSY;
	}
	s_pGpioRegisters = (struct GpioRegisters *)__io_address(GPIO_BASE);
	SetGPIOFunction(LedGpioPin, 0b001); //configure the pin as output
	SetGPIOFunction(ClockGpioPin, 0b001); //configure the pin as output
	SetGPIOOutputValue(LedGpioPin, false); //turn pin off  
	SetGPIOOutputValue(ClockGpioPin, false); //turn pin off  

	setup_timer(&s_BlinkTimer, BlinkTimerHandler, 0);
	setup_timer(&clockTimer ,ClockTimerHandler, 0);

	pr_info("Succeeded in registering character device %s\n", MYDEV_NAME);	
	return 0;
}

static void __exit LedBlinkModule_exit(void)
{

	SetGPIOFunction(LedGpioPin, 0); //configure the pin as input
	SetGPIOFunction(ClockGpioPin, 0); //configure the pin as input
	del_timer(&s_BlinkTimer);
	del_timer(&clockTimer);
	misc_deregister(&my_misc_device);
	pr_info("\ndevice unregistered\n");
	kfree(ramdisk);
}

static const struct file_operations mycdrv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mycdrv_unlocked_ioctl,
	.open = mycdrv_generic_open,
	.release = mycdrv_generic_release
};

module_init(LedBlinkModule_init);
module_exit(LedBlinkModule_exit);

MODULE_AUTHOR("Julie Anderson");
MODULE_LICENSE("GPL v2");
