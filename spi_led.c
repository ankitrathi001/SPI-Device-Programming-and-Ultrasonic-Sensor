/***********************************************************************
 *
 * File Name: spi_led.c
 *
 * Author: Ankit Rathi (ASU ID: 1207543476)
 * 			(Ankit.Rathi@asu.edu)
 *
 * Date: 30-OCT-2014
 *
 * Description: A driver program to control SPI-based display
 * 
 **********************************************************************/

/**
*Include Library Headers
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/kthread.h>

/**
 * Define constants using the macro
 */
#define DRIVER_NAME 		"spidev"
#define DEVICE_NAME 		"spidev"
#define DEVICE_CLASS_NAME 	"spidev"
#define MINOR_NUMBER    0
#define MAJOR_NUMBER    154     /* assigned */

#define GPIO42 42
#define GPIO43 43
#define GPIO54 54
#define GPIO55 55

static DEFINE_MUTEX(device_list_lock);

/**
 * per device structure
 */
struct spidev_data {
	dev_t                   devt;
	struct spi_device       *spi;
	char pattern_buffer[10][8];
	unsigned int sequence_buffer[10][2];
};

/**
 * Global variables
 */
static struct spidev_data *spidev_global;
static struct class *spi_led_class;   	/* Device class */
static unsigned bufsiz = 4096;
static unsigned int busyFlag=0;
static struct spi_message m;
static unsigned char ch_tx[2]={0};
static unsigned char ch_rx[2]={0};
static struct spi_transfer t = {
			.tx_buf = &ch_tx[0],
			.rx_buf = &ch_rx[0],
			.len = 2,
			.cs_change = 1,
			.bits_per_word = 8,
			.speed_hz = 500000,
			 };

/***********************************************************************
* spi_led_transfer - This function is used to transfer data to the spi
* 	bus.
* 
* @ch1: Address
* @ch2: Data
*
* Returns: -
* 
* Description: This function is used to transfer data to the spi
* 	bus.
***********************************************************************/
static void spi_led_transfer(unsigned char ch1, unsigned char ch2)
{
    int ret=0;
    ch_tx[0] = ch1;
    ch_tx[1] = ch2;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	ret = spi_sync(spidev_global->spi, &m);
	return;
}

/***********************************************************************
* spi_led_open - This function is called when the device is first 
* 	opened.
* 
* @inode: Inode Structure
* @filp: File Pointer
*
* Returns: 0
* 
* Description: This function is called when the device is first 
* 	opened. It does the initial setup needed to drive the LED Display
* 	for further patterns to be displayed.
***********************************************************************/
static int spi_led_open(struct inode *inode, struct file *filp)
{
	unsigned char i=0;
	busyFlag = 0;
	//printk("spi_led_open Start\n");
	spi_led_transfer(0x0F, 0x01);
	spi_led_transfer(0x0F, 0x00);
	spi_led_transfer(0x09, 0x00);
	spi_led_transfer(0x0A, 0x04);
	spi_led_transfer(0x0B, 0x07);
	spi_led_transfer(0x0C, 0x01);

	//Clear the LED Display
	for(i=1; i < 9; i++)
	{
		spi_led_transfer(i, 0x00);
	}
	
	//printk("spi_led_open End\n");
	return 0;
}

/***********************************************************************
* spi_led_release - This function is called to release all data
* 	structures that were used up by open function.
* 
* @inode: Inode Structure.
* @filp: File Pointer.
*
* Returns: 0 on success
* 
* Description: This function is called to release all data
* 	structures that were used up by open function.
***********************************************************************/
static int spi_led_release(struct inode *inode, struct file *filp)
{
    int status = 0;
    unsigned char i=0;
    busyFlag = 0;
    //Clear the LED Display
	for(i=1; i < 9; i++)
	{
		spi_led_transfer(i, 0x00);
	}
	
	gpio_free(GPIO42);
	gpio_free(GPIO43);
	gpio_free(GPIO54);
	gpio_free(GPIO55);
	
	printk("spi_led_release -- spidev is closing\n");
	return status;
}

/***********************************************************************
* thread_spi_led_write - This function is called by the kthread to write
* patterns to the LED Display.
* 
* @data: Data POinter.
*
* Returns: 0 on success
* 
* Description:  This function is called by the kthread to write
* patterns to the LED Display.
***********************************************************************/
int thread_spi_led_write(void *data)
{
	int i=0, j=0, k=0;
	//printk("\n\n thread_spi_led_write \n\n");
	
	if(spidev_global->sequence_buffer[0][0] == 0 && spidev_global->sequence_buffer[0][1] == 0)
	{
		for(k=1; k < 9; k++)
		{
			spi_led_transfer(k, 0x00);
		}
		busyFlag = 0;
		goto sequenceEnd;
	}
				
	//If sequence pattern followed by 0,0 is present, then display the pattern in loop upto 0,0.
	for(j=0;j<10;j++) //loop for sequence order
	{
		for(i=0;i<10;i++)//loop for pattern number
		{
			if(spidev_global->sequence_buffer[j][0] == i)
			{
				if(spidev_global->sequence_buffer[j][0] == 0 && spidev_global->sequence_buffer[j][1] == 0)
				{
					/*
					for(k=1; k < 9; k++)
					{
						spi_led_transfer(k, 0x00);
					}
					* */
					busyFlag = 0;
					goto sequenceEnd;
				}
				else
				{
					spi_led_transfer(0x01, spidev_global->pattern_buffer[i][0]);
					spi_led_transfer(0x02, spidev_global->pattern_buffer[i][1]);
					spi_led_transfer(0x03, spidev_global->pattern_buffer[i][2]);
					spi_led_transfer(0x04, spidev_global->pattern_buffer[i][3]);
					spi_led_transfer(0x05, spidev_global->pattern_buffer[i][4]);
					spi_led_transfer(0x06, spidev_global->pattern_buffer[i][5]);
					spi_led_transfer(0x07, spidev_global->pattern_buffer[i][6]);
					spi_led_transfer(0x08, spidev_global->pattern_buffer[i][7]);
					msleep(spidev_global->sequence_buffer[j][1]);
				}
			}
		}
	}
	sequenceEnd:
	busyFlag = 0;
	return 0;
}
/***********************************************************************
* spi_led_write - This function is used to send data over SPI bus to the
* 	LED Display.
* 
* @filp: File Pointer.
* @buf: Buffer
* @count: Size of Buffer
* @ppos: Position Pointer
*
* Returns: 0 on success
* 
* Description: This function is used to send data over SPI bus to the
* 	LED Display.
***********************************************************************/
static ssize_t spi_led_write(struct file *filp, const char *buf, size_t count, loff_t *ppos)
{
	int retValue = 0, i=0, j=0;
	unsigned  int sequenceBuffer[20];
	struct task_struct *task;
	//printk("\n\n spi_led_write \n\n");
	/* chipselect only toggles at start or end of operation */
	if(busyFlag == 1)
	{
		return -EBUSY;
	}
	if (count > bufsiz)
	{
		return -EMSGSIZE;
	}
	retValue = copy_from_user((void *)&sequenceBuffer, (void * __user)buf, sizeof(sequenceBuffer));
	for(i=0;i<20;i=i+2)
	{
		j=i/2;
		spidev_global->sequence_buffer[j][0] = sequenceBuffer[i];
		spidev_global->sequence_buffer[j][1] = sequenceBuffer[i+1];
	}
	if(retValue != 0)
	{
		printk("Failure : %d number of bytes that could not be copied.\n",retValue);
	}
	
	busyFlag = 1;

    task = kthread_run(&thread_spi_led_write, (void *)sequenceBuffer,"kthread_spi_led");

	return retValue;
}

/***********************************************************************
* spi_led_ioctl - This function is used to set the buffer with user
* 	defined patterns.
* 
* @filp: File Pointer.
* @arg: Input Arguments
* @cmd: Command
*
* Returns: 0 on success
* 
* Description: This function is used to set the buffer with user
* 	defined patterns.
***********************************************************************/
static long spi_led_ioctl(struct file *filp, unsigned int arg, unsigned long cmd)
{
	int i=0, j=0;
	char writeBuffer[10][8];
	int retValue;
    //printk("spi_led_ioctl Start\n");
	retValue = copy_from_user((void *)&writeBuffer, (void * __user)arg, sizeof(writeBuffer));
	if(retValue != 0)
	{
		printk("Failure : %d number of bytes that could not be copied.\n",retValue);
	}
	for(i=0;i<10;i++)
	{
		for(j=0;j<8;j++)
		{
			spidev_global->pattern_buffer[i][j] = writeBuffer[i][j];
		}
	}
	//printk("spi_led_ioctl End\n");
	return retValue;
}

/***********************************************************************
* Driver entry points 
***********************************************************************/
static struct file_operations spi_led_fops = {
  .owner   			= THIS_MODULE,
  .write   			= spi_led_write,
  .open    			= spi_led_open,
  .release 			= spi_led_release,
  .unlocked_ioctl   = spi_led_ioctl,
};

/***********************************************************************
* spidev_probe - This is the probe function. It gets called when device
* 	is to be initiallized or new device is being getting added.
* 
* @spi: SPI Device Structure
*
* Returns: 0 on success
* 
* Description: This is the probe function. It gets called when device
* 	is to be initiallized or new device is being getting added.
***********************************************************************/
static int spidev_probe(struct spi_device *spi)
{
	//struct spidev_data *spidev;
	int status = 0;
	struct device *dev;

	/* Allocate driver data */
	spidev_global = kzalloc(sizeof(*spidev_global), GFP_KERNEL);
	if(!spidev_global)
	{
		return -ENOMEM;
	}

	/* Initialize the driver data */
	spidev_global->spi = spi;

	spidev_global->devt = MKDEV(MAJOR_NUMBER, MINOR_NUMBER);

    dev = device_create(spi_led_class, &spi->dev, spidev_global->devt, spidev_global, DEVICE_NAME);

    if(dev == NULL)
    {
		printk("Device Creation Failed\n");
		kfree(spidev_global);
		return -1;
	}
	printk("SPI LED Driver Probed.\n");
	return status;
}

/***********************************************************************
* spidev_probe - This is the remove function. It gets called when device
* 	is disconnected.
* 
* @spi: SPI Device Structure
*
* Returns: 0 on success
* 
* Description: This is the remove function. It gets called when device
* 	is disconnected. So all the data structures that was allocated to it
* 	needs to be freed.
***********************************************************************/
static int spidev_remove(struct spi_device *spi)
{
	int retValue=0;
	
	device_destroy(spi_led_class, spidev_global->devt);
	kfree(spidev_global);
	printk("SPI LED Driver Removed.\n");
	return retValue;
}

static struct spi_driver spi_led_driver = {
         .driver = {
                 .name =         DRIVER_NAME,
                 .owner =        THIS_MODULE,
         },
         .probe =        spidev_probe,
         .remove =       spidev_remove,
};

/***********************************************************************
* spi_led_init - This function is called to initialize the SPI Bus and
* 	LED Display. 
* 	sensor.
* 
* Returns 0 on success
* 
* Description: This function is called to initialize the SPI Bus and
* 	LED Display. 
***********************************************************************/
static int __init spi_led_init(void)
{
	int retValue;
	
	//Register the Device
	retValue = register_chrdev(MAJOR_NUMBER, DEVICE_NAME, &spi_led_fops);
	if(retValue < 0)
	{
		printk("Device Registration Failed\n");
		return -1;
	}
	
	//Create the class
	spi_led_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);
	if(spi_led_class == NULL)
	{
		printk("Class Creation Failed\n");
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
	}
	
	//Register the Driver
	retValue = spi_register_driver(&spi_led_driver);
	if(retValue < 0)
	{
		printk("Driver Registraion Failed\n");
		class_destroy(spi_led_class);
		unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
		return -1;
	}
	
	//Free the GPIO Pins
	gpio_free(GPIO42);
	gpio_free(GPIO43);
	gpio_free(GPIO54);
	gpio_free(GPIO55);
	
	//Set the direction and Value of GPIO Pins
	gpio_request_one(GPIO42, GPIOF_OUT_INIT_LOW , "gpio42");
	gpio_request_one(GPIO43, GPIOF_OUT_INIT_LOW , "gpio43");
	gpio_request_one(GPIO54, GPIOF_OUT_INIT_LOW , "gpio54");
	gpio_request_one(GPIO55, GPIOF_OUT_INIT_LOW , "gpio55");
	
	//Set the Value of GPIO Pins	
	gpio_set_value_cansleep(GPIO42, 0);
	gpio_set_value_cansleep(GPIO43, 0);
	gpio_set_value_cansleep(GPIO54, 0);
	gpio_set_value_cansleep(GPIO55, 0);
	
	printk("SPI LED Driver Initialized.\n");
	return retValue;
}

/***********************************************************************
* spi_led_exit - This function is called when the driver is about to 
* exit.	It is cleaning fuction to clean open function functionalities.
* 
* Returns 0 on success
* 
* Description: This function is called when the driver is about to exit.
* 	It is cleaning fuction to clean open function functionalities.
***********************************************************************/
static void __exit spi_led_exit(void)
{
	spi_unregister_driver(&spi_led_driver);
	class_destroy(spi_led_class);
	unregister_chrdev(MAJOR_NUMBER, spi_led_driver.driver.name);
	printk("SPI LED Driver Uninitialized.\n");
}

MODULE_AUTHOR("Ankit Rathi, <Ankit.Rathi@asu.edu>");
MODULE_DESCRIPTION("SPI LED Driver");
MODULE_LICENSE("GPL");

module_init(spi_led_init);
module_exit(spi_led_exit);
