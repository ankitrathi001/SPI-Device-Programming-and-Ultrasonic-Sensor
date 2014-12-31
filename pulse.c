/***********************************************************************
 *
 * File Name: pulse.c
 *
 * Author: Ankit Rathi (ASU ID: 1207543476)
 * 			(Ankit.Rathi@asu.edu)
 *
 * Date: 30-OCT-2014
 *
 * Description: A driver program to control Ultrasonic Sensor Pulse 
 * 			measurements.
 * 
 **********************************************************************/

/**
*Include Library Headers
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <asm/errno.h>
#include <linux/math64.h>

/**
 * Define constants using the macro
 */
#define DRIVER_NAME 		"pulse"
#define DEVICE_NAME 		"pulse"
#define PULSE_MINOR_NUMBER 	0
#define GP_IO2 14  			//GPIO14 corresponds to IO2
#define GP_IO3 15  			//GPIO15 corresponds to IO3
#define GP_IO2_MUX 31  		//GPIO31 corresponds to MUX controlling IO2
#define GP_IO3_MUX 30  		//GPIO30 corresponds to MUX controlling IO2
#define GPIO_DIRECTION_IN 1
#define GPIO_DIRECTION_OUT 0
#define GPIO_VALUE_LOW 0
#define GPIO_VALUE_HIGH 1
#define RISE_DETECTION 0
#define FALL_DETECTION 1
static dev_t pulse_dev_number;      /* Allotted Device Number */
static struct class *pulse_class;   /* Device class */
static unsigned char Edge = RISE_DETECTION;

/**
 * per device structure
 */
typedef struct Pulse_Device_Tag
{
	struct cdev cdev;               /* The cdev structure */
	char name[20];                  /* Name of device */
	unsigned int BUSY_FLAG;		  	/* Busy Flag Status */
	unsigned long long timeRising;		/* TimeStamp to record Start Time */
	unsigned long long timeFalling;		/* TimeStamp to record End Time */
	int irq;
} Pulse_Device;

Pulse_Device *pulse_dev;

/**
 * rdtsc() function is used to calulcate the number of clock ticks
 * and measure the time. TSC(time stamp counter) is incremented 
 * every cpu tick (1/CPU_HZ).
 * 
 * Source: http://www.mcs.anl.gov/~kazutomo/rdtsc.html
 */
static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long) lo) | ((unsigned long long) hi)<<32;
}

/***********************************************************************
* change_state_interrupt - This is Interrrupt Handler function.
* @data: Thread Parameters
*
* Returns IRQ_HANDLED
* 
* Description: This is Interrrupt Handler function. It checks for rising
* 	and falling edges and notes down the time when rising edge arrived
* 	and time when falling edge arrived.
***********************************************************************/
static irqreturn_t change_state_interrupt(int irq, void *dev_id)
{
	//printk("pulse.c change_state_interrupt Start\n");
	if(Edge==RISE_DETECTION)
	{
		pulse_dev->timeRising = rdtsc();
	    irq_set_irq_type(irq, IRQF_TRIGGER_FALLING);
	    Edge=FALL_DETECTION;
	}
	else
	{
		pulse_dev->timeFalling = rdtsc();
	    irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
	    Edge=RISE_DETECTION;
		pulse_dev->BUSY_FLAG = 0;
	}
	//printk("pulse.c change_state_interrupt End\n");
	return IRQ_HANDLED;
}

/***********************************************************************
* pulse_open - This is function that will be called when the device is
* 	opened.
* @inode: Inode structure
* @filp: File Pointer
* 
* Returns 0 on success
* 
* Description: This is function that will be called when the device is 
* 	opened.
***********************************************************************/
int pulse_open(struct inode *inode, struct file *filp)
{
	int irq_line;
	int irq_req_res_rising;
	int retValue;
	
	//printk("pulse.c pulse_open() Start \n");
	pulse_dev->BUSY_FLAG = 0;
	
	/* Get the per-device structure that contains this cdev */
	pulse_dev = container_of(inode->i_cdev, Pulse_Device, cdev);
	
	/* Easy access to cmos_devp from rest of the entry points */
	filp->private_data = pulse_dev;
	
	//Free the GPIO Pins
	gpio_free(GP_IO2);
	gpio_free(GP_IO3);
	gpio_free(GP_IO2_MUX);
	gpio_free(GP_IO3_MUX);
	
	//Set GPIO pins directions and values
	gpio_request_one(GP_IO2_MUX, GPIOF_OUT_INIT_LOW , "gpio31");
	gpio_request_one(GP_IO3_MUX, GPIOF_OUT_INIT_LOW , "gpio30");
	gpio_request_one(GP_IO2, GPIOF_OUT_INIT_LOW , "gpio14");
	gpio_request_one(GP_IO3, GPIOF_OUT_INIT_LOW , "gpio15");
	
	//Set GPIO pins values
	gpio_set_value_cansleep(GP_IO2, GPIO_VALUE_LOW);
	gpio_set_value_cansleep(GP_IO3, GPIO_VALUE_LOW);
	gpio_set_value_cansleep(GP_IO2_MUX, GPIO_VALUE_LOW);
	gpio_set_value_cansleep(GP_IO3_MUX, GPIO_VALUE_LOW);
	
	//printk("%s has opened\n", pulse_dev->name);
	gpio_free(GP_IO3);
	gpio_request_one(GP_IO3, GPIOF_IN , "gpio15");
	
	/*install interrupt handler*/
	irq_line = gpio_to_irq(GP_IO3);
	
	if(irq_line < 0)
	{
		printk("Gpio %d cannot be used as interrupt",GP_IO3);
		retValue=-EINVAL;
	}
	pulse_dev->irq = irq_line;
	
	pulse_dev->timeRising=0;
	pulse_dev->timeFalling=0;
	
	irq_req_res_rising = request_irq(irq_line, change_state_interrupt, IRQF_TRIGGER_RISING, "gpio_change_state", pulse_dev);
	if(irq_req_res_rising)
	{
		printk("Unable to claim irq %d; error %d\n ", irq_line, irq_req_res_rising);
		return 0;
	}
	
	//printk("pulse.c pulse_open() End \n");
	return 0;
}

/***********************************************************************
* pulse_release - This is is used by the driver to close anything which 
* 	has been opened and used during driver execution.
* 
* @inode: Inode structure
* @filp: File Pointer
* 
* Returns 0 on success
* 
* Description: This is is used by the driver to close anything which 
* 	has been opened and used during driver execution.
***********************************************************************/
int pulse_release(struct inode *inode, struct file *filp)
{
	Pulse_Device *local_pulse_dev;
	//printk("pulse.c pulse_release() Start\n");
	
	pulse_dev->BUSY_FLAG = 0;
	local_pulse_dev = filp->private_data;
	free_irq(pulse_dev->irq,pulse_dev);
	
	gpio_free(GP_IO2);
	gpio_free(GP_IO3);
	gpio_free(GP_IO2_MUX);
	gpio_free(GP_IO3_MUX);
	
	printk("pulse_release -- %s is closing\n", local_pulse_dev->name);
	//printk("pulse.c pulse_release() End\n");
	return 0;
}

/***********************************************************************
* pulse_write - This function is used to send the trigger pulse to the
* 	sensor.
* 
* @filp: File Pointer
* @buf: Buffer
* @count: Size of Buffer
* @ppos: Position Pointer
* 
* Returns 0 on success
* 
* Description: This function is used to send the trigger pulse to the
* 	sensor.
***********************************************************************/
static ssize_t pulse_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	int retValue = 0;
	//printk("pulse.c pulse_write() Start\n");
	
	if(pulse_dev->BUSY_FLAG == 1)
	{
		retValue = -EBUSY;
		return -EBUSY;
	}
	
	//Generate a trigger pulse
	gpio_set_value_cansleep(GP_IO2, GPIO_VALUE_HIGH);
	udelay(18);
	gpio_set_value_cansleep(GP_IO2, GPIO_VALUE_LOW);
	pulse_dev->BUSY_FLAG = 1;
	//printk("pulse.c pulse_write() End\n");
	return retValue;
}

/***********************************************************************
* pulse_read - This function is used by the user application to measure
* 	the pulse width. That is it measures the distance of object from the
* 	sensor.
* 
* @file: File Structure
* @buf: Buffer
* @count: Size of Buffer
* @ptr: Position Pointer
* 
* Returns 0 on success
* 
* Description: This function is used by the user application to measure
* 	the pulse width. That is it measures the distance of object from the
* 	sensor.
***********************************************************************/
static ssize_t pulse_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
	int retValue=0;
	unsigned int c;
	unsigned long long tempBuffer;
	//printk("pulse.c pulse_read() Start\n");
	if(pulse_dev->BUSY_FLAG == 1)
	{
		return -EBUSY;
	}
	else
	{
		if(pulse_dev->timeRising == 0 && pulse_dev->timeFalling == 0)
		{
			printk("Please Trigger the measure first\n");
		}
		else
		{
			tempBuffer = pulse_dev->timeFalling - pulse_dev->timeRising;
			c = div_u64(tempBuffer,400);
			retValue = copy_to_user((void *)buf, (const void *)&c, sizeof(c));
		}
	}
	//printk("pulse.c pulse_read() End\n");
	return retValue;
}

/**
 * File operations structure. Defined in linux/fs.h
 */
static struct file_operations pulse_fops =
{
		.owner = THIS_MODULE,			/* Owner */
		.open = pulse_open,             /* Open method */
		.release = pulse_release,       /* Release method */
		.write = pulse_write,           /* Write method */
		.read = pulse_read				/* Read method */
};

/***********************************************************************
* pulse_init - This function is called to initialize the Ultrasonic 
* 	sensor.
* 
* Returns 0 on success
* 
* Description: This function is called to initialize the Ultrasonic 
* 	sensor.
***********************************************************************/
static int __init pulse_init(void)
{
	int retValue;
	//printk("pulse.c pulse_init() Start \n");
	
	/* Request dynamic allocation of a device major number */
	if(alloc_chrdev_region(&pulse_dev_number, 0, PULSE_MINOR_NUMBER, DRIVER_NAME) < 0)
	{
		printk("Can't register device\n");
		return -1;
	}
	
	/* Populate sysfs entries */
	pulse_class = class_create(THIS_MODULE, DRIVER_NAME);
	
	/* Allocate memory for the per-device structure pulse_dev */
	pulse_dev = kmalloc(sizeof(Pulse_Device), GFP_KERNEL);
	if(!pulse_dev)
	{
		printk("Bad Kmalloc pulse_dev\n");
		return -ENOMEM;
	}

	/* Request I/O Region */
	sprintf(pulse_dev->name, DRIVER_NAME);

	/* Connect the file operations with the cdev */
	cdev_init(&pulse_dev->cdev, &pulse_fops);
	pulse_dev->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	retValue = cdev_add(&pulse_dev->cdev, MKDEV(MAJOR(pulse_dev_number), PULSE_MINOR_NUMBER), 1);
	if(retValue)
	{
		printk("Bad cdev for pulse_dev\n");
		return retValue;
	}
	
	/* A struct device will be created in sysfs, registered to the specified class.*/
	device_create(pulse_class, NULL, MKDEV(MAJOR(pulse_dev_number), PULSE_MINOR_NUMBER), NULL, DEVICE_NAME);
	
	printk("Pulse Driver = %s Initialized.\n", DRIVER_NAME);
	//printk("pulse.c pulse_init() Ends \n");
	return 0;
}

/***********************************************************************
* pulse_exit - This function is called when the driver is about to exit.
* 	It is cleaning fuction to clean open function functionalities.
* 
* Returns 0 on success
* 
* Description: This function is called when the driver is about to exit.
* 	It is cleaning fuction to clean open function functionalities.
***********************************************************************/
static void __exit pulse_exit(void)
{
	//printk("pulse_exit() Start\n");
	
	/* Destroy device with Minor Number 0*/
	device_destroy(pulse_class, MKDEV(MAJOR(pulse_dev_number), PULSE_MINOR_NUMBER));
	cdev_del(&pulse_dev->cdev);
	kfree(pulse_dev);
	
	/* Destroy driver_class */
	class_destroy(pulse_class);

	/* Release the major number */
	unregister_chrdev_region(pulse_dev_number, 1);
	printk("Pulse Driver = %s Uninitialized.\n", DRIVER_NAME);
	//printk("pulse_exit() End\n");
}

module_init(pulse_init);
module_exit(pulse_exit);

MODULE_AUTHOR("Ankit Rathi, <Ankit.Rathi@asu.edu>");
MODULE_DESCRIPTION("PULSE Driver");
MODULE_LICENSE("GPL");
