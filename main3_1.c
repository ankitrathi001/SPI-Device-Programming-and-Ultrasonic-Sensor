/***********************************************************************
 *
 * File Name: main3_1.c
 *
 * Author: Ankit Rathi (ASU ID: 1207543476)
 * 			(Ankit.Rathi@asu.edu)
 *
 * Date: 04-NOV-2014
 *
 * Description: A test program to test the working scenarios of LED and 
 * Ultrasonic sensor using the inbuilt drivers.
 **********************************************************************/
 
/**
*	Include Library Headers 
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

/**
 * Define constants using the macro
 */ 
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define POLL_TIMEOUT 3000 /* 3 seconds */
#define GP_IO2 14  //GPIO14 corresponds to IO2
#define GP_IO3 15  //GPIO15 corresponds to IO3
#define GP_IO2_MUX 31  //GPIO31 corresponds to MUX controlling IO2
#define GP_IO3_MUX 30  //GPIO30 corresponds to MUX controlling IO2
#define MAX_BUF 64
#define CPU_CLOCK_SPEED 400000000.00 //400 MHz
#define GPIO_DIRECTION_IN 1
#define GPIO_DIRECTION_OUT 0
#define GPIO_VALUE_LOW 0
#define GPIO_VALUE_HIGH 1
#define SPI_DEVICE_NAME "/dev/spidev1.0"
#define GPIO_SPI1_CS_IO10 	10
#define GPIO_SPI1_MOSI_IO11 25
#define GPIO_SPI1_MISO_IO12 38
#define GPIO_SPI1_SCK_IO13 	39
#define GPIO42 42
#define GPIO43 43
#define GPIO54 54
#define GPIO55 55

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static uint8_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;

double distance;

/**
 * mutex for protecting
 */
pthread_mutex_t mutex;
 
 /**
 * Thread Arguments
 */
typedef struct 
{
	int threadId;
	int fd;
}ThreadParams;

typedef unsigned long long u64;
typedef unsigned long      u32;

/***********************************************************************
 * rdtsc() function is used to calulcate the number of clock ticks
 * and measure the time. TSC(time stamp counter) is incremented 
 * every cpu tick (1/CPU_HZ).
 **********************************************************************/
static inline u64 rdtsc(void)
{
	if (sizeof(long) == sizeof(u64))
	{
		u32 lo, hi;
		asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
		return ((u64)(hi) << 32) | lo;
	}
	else
	{
		u64 tsc;
		asm volatile("rdtsc" : "=A" (tsc));
		return tsc;
	}
}

/***********************************************************************
* gpio_export - Function to export gpio pins.
* @gpio: GPIO PIN Number
*
* Returns 0 on success.
* 
* Description: Function to export gpio pins.
***********************************************************************/
int gpio_export(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if(fd < 0)
	{
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
 
	return 0;
}

/***********************************************************************
* gpio_export - Function to unexport gpio pins.
* @gpio: GPIO PIN Number
*
* Returns 0 on success.
* 
* Description: Function to unexport gpio pins.
***********************************************************************/
int gpio_unexport(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];
 
	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if(fd < 0)
	{
		perror("gpio/export");
		return fd;
	}
 
	len = snprintf(buf, sizeof(buf), "%d", gpio);
	write(fd, buf, len);
	close(fd);
	return 0;
}

/***********************************************************************
* gpio_set_dir - Function to set directions for gpio pins.
* @gpio: GPIO PIN Number
* @out_flag: Directions of GPIO PIN
*
* Returns 0 on success.
* 
* Description: Function to set directions for gpio pins.
***********************************************************************/
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
	int fd, len;
	char buf[MAX_BUF];
 
	len = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", gpio);
 
	fd = open(buf, O_WRONLY);
	if(fd < 0)
	{
		perror("gpio/direction");
		return fd;
	}
 
	if (out_flag == GPIO_DIRECTION_OUT)
		write(fd, "out", 4);
	else
		write(fd, "in", 3);
 
	close(fd);
	return 0;
}

/***********************************************************************
* gpio_set_value - Function to set value for gpio pins.
* @gpio: GPIO PIN Number
* @value: value of GPIO PIN
*
* Returns 0 on success.
* 
* Description: Function to set value for gpio pins.
***********************************************************************/
int gpio_set_value(unsigned int gpio, unsigned int value)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpio);

	fd = open(buf, O_WRONLY);
	if(fd < 0)
	{
		perror("gpio/set-value");
		return fd;
	}

	if(value == GPIO_VALUE_HIGH)
		write(fd, "1", 2);
	else
		write(fd, "0", 2);

	close(fd);
	return 0;
}

/***********************************************************************
* gpio_get_value - Function to get value for gpio pins.
* @gpio: GPIO PIN Number
* @value: value of GPIO PIN
*
* Returns 0 on success.
* 
* Description: Function to get value for gpio pins.
***********************************************************************/
int gpio_get_value(unsigned int gpio, unsigned int *value)
{
	int fd, len;
	char buf[MAX_BUF];
	char ch;

	len = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY);
	if (fd < 0) {
		perror("gpio/get-value");
		return fd;
	}
 
	read(fd, &ch, 1);

	if (ch != '0') {
		*value = 1;
	} else {
		*value = 0;
	}
 
	close(fd);
	return 0;
}

/***********************************************************************
* gpio_set_edge - Function to set edge for gpio pins.
* @gpio: GPIO PIN Number
* @edge: edge of GPIO PIN
*
* Returns 0 on success.
* 
* Description: Function to set edge for gpio pins.
***********************************************************************/
int gpio_set_edge(unsigned int gpio, char *edge)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/edge", gpio);
 
	fd = open(buf, O_WRONLY);
	if(fd < 0)
	{
		perror("gpio/set-edge");
		return fd;
	}
 
	write(fd, edge, strlen(edge)+1); 
	close(fd);
	return 0;
}

/***********************************************************************
* gpio_fd_open - Function to create file descriptor for the gpio.
* @gpio: GPIO PIN Number
*
* Returns fd on success.
* 
* Description: Function to create file descriptor for the gpio.
***********************************************************************/
int gpio_fd_open(unsigned int gpio)
{
	int fd, len;
	char buf[MAX_BUF];

	len = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", gpio);
 
	fd = open(buf, O_RDONLY | O_NONBLOCK );
	if (fd < 0) {
		perror("gpio/fd_open");
	}
	return fd;
}

/***********************************************************************
* gpio_fd_open - Function to close file descriptor for the gpio.
* @gpio: GPIO PIN Number
*
* Returns 0 on success.
* 
* Description: Function to close file descriptor for the gpio.
***********************************************************************/
int gpio_fd_close(int fd)
{
	return close(fd);
}

/***********************************************************************
* transfer - Function to create a transfer structure and pass it to 
* 		IOCTL to display on LED.
* @address: Address
* @data: data
*
* Returns 0 on success.
* 
* Description: Function to create a transfer structure and pass it to 
* 		IOCTL to display on LED. Various parameters for the transfer are
* 		set like, bits per word, speed etc.
***********************************************************************/
void transfer(int fd, uint8_t address, uint8_t data)
{
	//printf("transfer start   fd %d\n",fd);
	int retValue,i;
	
	uint8_t tx[2];
	tx[0] = address;
	tx[1] = data;
	uint8_t rx[ARRAY_SIZE(tx)] = {0, };
	
	
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = ARRAY_SIZE(tx),
		.cs_change = 1,
		.speed_hz = speed,
		.bits_per_word = bits,
	};
	
	retValue = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	//printf("retValue = %d\n",retValue);
	if(retValue < 0)
	{
		printf("error in sending message\n");
	}
	//printf("transfer end\n");
}

/***********************************************************************
* clearLEDDisplay - Function to clear the LED Display.
* @fd: file descriptor
*
* Returns 0 on success.
* 
* Description: Function to clear the LED Display.
***********************************************************************/
void clearLEDDisplay(int fd)
{
	char i;
	for(i=1; i < 9; i++)
	{
		transfer(fd, i, 0x00);
	}
}
/***********************************************************************
* initLEDDisplay - Function to initialize the LED Display.
* @fd: file descriptor
*
* Returns 0 on success.
* 
* Description: Function to initialize the LED Display. Here the intesity
* 		Mode of operation, scan digits are set.
***********************************************************************/
void initLEDDisplay(int fd)
{
	usleep(100000);
	// Enable mode B
	transfer(fd, 0x09, 0x00);
	usleep(100000);
	// Define Intensity
	transfer(fd, 0x0A, 0x04);
	usleep(100000);
	// Only scan 7 digit
	transfer(fd, 0x0B, 0x07);
	usleep(100000);
	// Turn on chip
	transfer(fd, 0x0C, 0x01);
	usleep(100000);
	clearLEDDisplay(fd);
}

/***********************************************************************
* thread_transmit_spi - Thread Function to send data to the LED display.
* @fd: file descriptor
*
* Returns 0 on success.
* 
* Description: Thread Function to send data to the LED display. We 
* 	continuosly monitor the distance obatined from the sensor, using 
* 	this distance, we can find if the person/obstance is approaching or 
* 	going away from sensor. Based on this the dog id made to run slow 
* 	and fast and turn its direction.
***********************************************************************/
void *thread_transmit_spi(void *data)
{
	int retValue,fd;
	ThreadParams *tparams = (ThreadParams*)data;
	double distance_previous = 0, distance_current = 0, distance_diff = 0, distance_threshhold=0;
	char new_direction = 'L', old_direction = 'L';
	
	//printf("thread_transmit_spi Start\n");

	fd = tparams->fd;
	
	//Unexport all GPIO Pins
	gpio_unexport(GPIO42);
	gpio_unexport(GPIO43);
	gpio_unexport(GPIO54);
	gpio_unexport(GPIO55);
	
	//Export all GPIO Pins
	gpio_export(GPIO42);
	gpio_export(GPIO43);
	gpio_export(GPIO54);
	gpio_export(GPIO55);
	
	//Set Directions for all GPIO Pins
	gpio_set_dir(GPIO42, GPIO_DIRECTION_OUT);
	gpio_set_dir(GPIO43, GPIO_DIRECTION_OUT);
	gpio_set_dir(GPIO54, GPIO_DIRECTION_OUT);
	gpio_set_dir(GPIO55, GPIO_DIRECTION_OUT);

	//Set Values for all GPIO Pins
	gpio_set_value(GPIO42, GPIO_VALUE_LOW);
	gpio_set_value(GPIO43, GPIO_VALUE_LOW);
	gpio_set_value(GPIO54, GPIO_VALUE_LOW);
	gpio_set_value(GPIO55, GPIO_VALUE_LOW);
	
	//Open the Device for File Operations
	fd = open(SPI_DEVICE_NAME, O_RDWR);

	if(fd < 0)
	{
		printf("Can not open device file fd_spi.\n");
		return 0;
	}
	else
	{
		//printf("fd_spi device opened succcessfully.\n");
	}
	
	/*
	* spi mode
	*/
	retValue = ioctl(fd, SPI_IOC_WR_MODE, &mode);
	if(retValue == -1)
	{
		printf("can't set spi mode\n");
	}
	retValue = ioctl(fd, SPI_IOC_RD_MODE, &mode);

	if(retValue == -1)
	{
		printf("can't get spi mode\n");
	}
		
	/*
	* bits per word
	*/
	retValue = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
	if (retValue == -1)
	{
		printf("can't set bits per word\n");
	}
	retValue = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
	if (retValue == -1)
	{
		printf("can't get bits per word\n");
	}

	/*
	* max speed hz
	*/
	retValue = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (retValue == -1)
	{
		printf("can't set max speed hz\n");
	}
	retValue = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	if (retValue == -1)
	{
		printf("can't get max speed hz\n");
	}

	
	initLEDDisplay(fd);

	usleep(100000);
	while(1)
	{
		//sleep(1);
		pthread_mutex_lock(&mutex);
		distance_current = distance;
		pthread_mutex_unlock(&mutex);
		distance_diff = distance_current - distance_previous;
		distance_threshhold = distance_current / 10.0;
		printf("Distance = %0.2f\n",distance_current);

		if((distance_diff > -distance_threshhold) && (distance_diff < distance_threshhold))
		{
			new_direction = old_direction;
		}
		else if(distance_diff > distance_threshhold)
		{
			new_direction = 'R';
		}
		else if(distance_diff < -distance_threshhold)
		{
			new_direction = 'L';
		}

		if(new_direction == 'R')
		{
			//printf("Moving Away... Move Right\n");
			transfer(fd, 0x01, 0x08);
			transfer(fd, 0x02, 0x90);
			transfer(fd, 0x03, 0xf0);
			transfer(fd, 0x04, 0x10);
			transfer(fd, 0x05, 0x10);
			transfer(fd, 0x06, 0x37);
			transfer(fd, 0x07, 0xdf);
			transfer(fd, 0x08, 0x98);
			//usleep(300000);
			if(distance_current < 200)
			{
				usleep(distance_current * 5000);
			}
			else
			{
				usleep(500 * 5000);
			}
			transfer(fd, 0x01, 0x20);
			transfer(fd, 0x02, 0x10);
			transfer(fd, 0x03, 0x70);
			transfer(fd, 0x04, 0xd0);
			transfer(fd, 0x05, 0x10);
			transfer(fd, 0x06, 0x97);
			transfer(fd, 0x07, 0xff);
			transfer(fd, 0x08, 0x18);
			
			//usleep(300000);
			if(distance_current < 200)
			{
				usleep(distance_current * 5000);
			}
			else
			{
				usleep(500 * 5000);
			}
		}
		else if(new_direction == 'L')
		{
			//printf("Moving Closer... Move Left\n");
			transfer(fd, 0x01, 0x98);
			transfer(fd, 0x02, 0xdf);
			transfer(fd, 0x03, 0x37);
			transfer(fd, 0x04, 0x10);
			transfer(fd, 0x05, 0x10);
			transfer(fd, 0x06, 0xf0);
			transfer(fd, 0x07, 0x90);
			transfer(fd, 0x08, 0x08);
			//usleep(300000);
			if(distance_current < 200)
			{
				usleep(distance_current * 5000);
			}
			else
			{
				usleep(500 * 5000);
			}
			transfer(fd, 0x01, 0x18);
			transfer(fd, 0x02, 0xff);
			transfer(fd, 0x03, 0x97);
			transfer(fd, 0x04, 0x10);
			transfer(fd, 0x05, 0xd0);
			transfer(fd, 0x06, 0x70);
			transfer(fd, 0x07, 0x10);
			transfer(fd, 0x08, 0x20);
			
			//usleep(300000);
			if(distance_current < 200)
			{
				usleep(distance_current * 5000);
			}
			else
			{
				usleep(500 * 5000);
			}
		}
			
		distance_previous = distance_current;
		old_direction = new_direction;
	}
	close(fd);
	//printf("thread_transmit_spi End\n");
	pthread_exit(0);
}


/***********************************************************************
* thread_Ultrasonic_distance - Thread Function to measure the distance
* 			using the pulsewidth obtained from sensor.
* @data: Thread Parameters
*
* Returns NULL
* 
* Description:  Thread Function to measure the distance. We continuosly
* 	poll the gpio pins for for rising and falling edge. At rising and 
* 	falling time, the time is noted down. This is then used to calulcate
*  	the distance from sensor.
***************************************e********************************/
void *thread_Ultrasonic_distance(void *data)
{
	struct pollfd poll_io3;
	int rc, retValue;
	unsigned long long timeRising,timeFalling;
	char *buf[MAX_BUF];
	
	//Unexport all GPIO Pins
	gpio_unexport(GP_IO2);
	gpio_unexport(GP_IO3);
	gpio_unexport(GP_IO2_MUX);
	gpio_unexport(GP_IO3_MUX);
	
	//Export all GPIO Pins
	gpio_export(GP_IO2);
	gpio_export(GP_IO3);
	gpio_export(GP_IO2_MUX);
	gpio_export(GP_IO3_MUX);
	
	//Set Directions for all GPIO Pins
	gpio_set_dir(GP_IO2, GPIO_DIRECTION_OUT);
	gpio_set_dir(GP_IO3, GPIO_DIRECTION_OUT);
	gpio_set_dir(GP_IO2_MUX, GPIO_DIRECTION_OUT);
	gpio_set_dir(GP_IO3_MUX, GPIO_DIRECTION_OUT);
	
	//Set values for all GPIO Pins
	gpio_set_value(GP_IO2,GPIO_VALUE_LOW);
	gpio_set_value(GP_IO3,GPIO_VALUE_LOW);
	gpio_set_value(GP_IO2_MUX,GPIO_VALUE_LOW);
	gpio_set_value(GP_IO3_MUX,GPIO_VALUE_LOW);
	
	gpio_set_dir(GP_IO3, GPIO_DIRECTION_IN);

	poll_io3.fd = gpio_fd_open(GP_IO3);
	poll_io3.events = POLLPRI|POLLERR;

	while(1)
	{
		lseek(poll_io3.fd, 0, SEEK_SET);
		gpio_set_edge(GP_IO3, "rising");
		//gpio_set_value(GP_IO2, GPIO_VALUE_HIGH);
		gpio_set_value(GP_IO2, GPIO_VALUE_LOW);
		usleep(2);
		gpio_set_value(GP_IO2, GPIO_VALUE_HIGH);
		usleep(12);
		gpio_set_value(GP_IO2, GPIO_VALUE_LOW);
//		usleep(2);

		rc = poll(&poll_io3, 1, POLL_TIMEOUT);
		
		if(rc < 0)
		{
			printf("poll() failed!!! Error Ocurred\n");
			//return -1;
		}
		if(rc == 0)
		{
			//printf("poll() timed out!!! No File Descriptors were ready\n");
		}
		if(rc > 0)
		{
			if(poll_io3.revents & POLLPRI)
			{
				timeRising = rdtsc();
				retValue = read(poll_io3.fd, buf, 1);
				if(retValue > 0)
				{
				}
			}
			else
			{
				printf("Error in detecting Rising Edge\n");
			}
		}
		
		lseek(poll_io3.fd, 0, SEEK_SET);
		gpio_set_edge(GP_IO3, "falling");
		
		rc = poll(&poll_io3, 1, POLL_TIMEOUT);
		
		if(rc < 0)
		{
			printf("poll() failed!!! Error Ocurred\n");
			//return -1;
		}
		if(rc == 0)
		{
			printf("poll() timed out!!! No File Descriptors were ready\n");
		}
		if(rc > 0)
		{
			if(poll_io3.revents & POLLPRI)
			{
				timeFalling = rdtsc();
				retValue = read(poll_io3.fd, buf, 1);
				if(retValue > 0)
				{
				}
			}
			else
			{
				printf("Error in detecting Falling Edge\n");
			}
		}
		usleep(500000);

		pthread_mutex_lock(&mutex);
		distance = ((timeFalling - timeRising) * 340.00) / (2.0 * 4000000);

		pthread_mutex_unlock(&mutex);
	}
	gpio_unexport(GP_IO2);
	gpio_unexport(GP_IO3);
	gpio_unexport(GP_IO2_MUX);
	gpio_unexport(GP_IO3_MUX);
	
	pthread_exit(0);
}

/***********************************************************************
* main - Main Thread Function which creates two threads, one to read the
* 		sensor and other to display data onto LED
* @data: Thread Parameters
*
* Returns NULL
* 
* Description:  Main Thread Function which creates two threads, one to 
* 	read the sensor and other to display data onto LED
***********************************************************************/
int main(int argc, char **argv, char **envp)
{
	int retValue;
	pthread_t thread_id_spi, thread_id_dist;
	ThreadParams *tp_spi, *tp_dist;

	/* Mutex Initialization*/
	pthread_mutex_init(&mutex, NULL);
	
	tp_spi = malloc(sizeof(ThreadParams));
	tp_spi -> threadId = 100;
	//tp_spi -> fd = fd_spi;
	retValue = pthread_create(&thread_id_spi, NULL, &thread_transmit_spi, (void*)tp_spi);
	if(retValue)
	{
		printf("ERROR; return code from pthread_create() is %d\n", retValue);
		exit(-1);
	}
	
	tp_dist = malloc(sizeof(ThreadParams));
	tp_dist -> threadId = 101;
	retValue = pthread_create(&thread_id_dist, NULL, &thread_Ultrasonic_distance, (void*)tp_dist);
	
	pthread_join(thread_id_spi, NULL);
	pthread_join(thread_id_dist, NULL);

	//close(fd_spi);
	return 0;
}
