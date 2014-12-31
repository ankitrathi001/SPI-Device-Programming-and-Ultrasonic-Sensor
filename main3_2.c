/***********************************************************************
 *
 * File Name: main3_2.c
 *
 * Author: Ankit Rathi (ASU ID: 1207543476)
 * 			(Ankit.Rathi@asu.edu)
 *
 * Date: 04-NOV-2014
 *
 * Description: A test program to test the working scenarios of LED and 
 * Ultrasonic sensor using the user created drivers.
 *
 **********************************************************************/
 
/**
 *Include Library Headers 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/**
 * Define constants using the macro
 */ 
#define SPI_DEVICE_NAME "/dev/spidev"
#define PULSE_DEVICE_NAME "/dev/pulse"

/**
* Thread Arguments
*/
typedef struct 
{
	int threadId;
	int fd;
}ThreadParams;


/**
 * mutex for protecting
 */
pthread_mutex_t mutex;

double distance;

/***********************************************************************
* thread_transmit_spi_dog - thread Function to send Dog display
* @data: Thread Parameters
*
* Returns NULL
* 
* Description: Thread Function to send Dog display
***********************************************************************/
void *thread_transmit_spi_dog(void *data)
{
	int retValue,fd;
	ThreadParams *tparams = (ThreadParams*)data;
	double distance_previous = 0, distance_current = 0, distance_diff = 0, distance_threshhold=0;
	char new_direction = 'L', old_direction = 'L';
	unsigned int timeToDisplay = 0;
	unsigned int sequenceBuffer[4];
	char patternBuffer[4][8] = {
		{0x08, 0x90, 0xf0, 0x10, 0x10, 0x37, 0xdf, 0x98},
		{0x20, 0x10, 0x70, 0xd0, 0x10, 0x97, 0xff, 0x18},
		{0x98, 0xdf, 0x37, 0x10, 0x10, 0xf0, 0x90, 0x08},
		{0x18, 0xff, 0x97, 0x10, 0xd0, 0x70, 0x10, 0x20},
		};
		
	//printf("thread_transmit_spi Start\n");

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
	
	spi_led_ioctl(fd,patternBuffer);
	while(1)
	{
		pthread_mutex_lock(&mutex);
		distance_current = distance;
		pthread_mutex_unlock(&mutex);
		distance_diff = distance_current - distance_previous;
		distance_threshhold = distance_current / 10.0;

		printf("Distance = %0.2f cm\n",distance_current);

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
		
		
		if(distance_current > 300)
		{
			timeToDisplay = 300 * 5;
		}
		else
		{
			timeToDisplay = distance_current * 5;
		}
		
		if(new_direction == 'R')
		{
			//printf("Moving Away... Move Right\n");
			sequenceBuffer[0] = 0;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);

			sequenceBuffer[0] = 1;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}
		else if(new_direction == 'L')
		{
			//printf("Moving Closer... Move Left\n");
			sequenceBuffer[0] = 2;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
			
			sequenceBuffer[0] = 3;
			sequenceBuffer[1] = timeToDisplay;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
		}
		distance_previous = distance_current;
		old_direction = new_direction;
		usleep(10000);
	}
	//printf("thread_transmit_spi fd = %d\n",fd);
	close(fd);
	pthread_exit(0);
}

/***********************************************************************
* thread_transmit_spi_number - Thread Function to display counter from 
* 		0 to 99. Resets back to 0 after 99.
* @data: Thread Parameters
*
* Returns NULL
* 
* Description:  Thread Function to display counter from 
* 		0 to 99. Resets back to 0 after 99. The counter speed is 
* 		controlled by the distance sensor. Closer you are to the sensor
* 		faster is the counting and away you are from the sensor, the 
* 		counter slows down.
***********************************************************************/
void *thread_transmit_spi_number(void *data)
{
	int retValue,fd;
	int i,j,k;
	int leftNumber = 0, rightNumber = 0;
	ThreadParams *tparams = (ThreadParams*)data;
	double distance_previous = distance, distance_current = distance, distance_diff = 0;
	char numberBuffer[10][4] = {
		{0x7e, 0x81, 0x81, 0x7e},
		{0x84, 0x82, 0xff, 0x80},
		{0x61, 0x91, 0x91, 0x8e},
		{0x81, 0x99, 0x99, 0x7e},
		{0x0f, 0x08, 0x08, 0xff},
		{0x8e, 0x91, 0x91, 0x61},
		{0x7e, 0x89, 0x89, 0x71},
		{0x01, 0x01, 0x01, 0xfe},
		{0x6e, 0x91, 0x91, 0x6e},
		{0x8e, 0x91, 0x91, 0x7e},
		};
	char tempBuffer[8];
	unsigned int sequenceBuffer[4];
	//printf("thread_transmit_spi Start\n");

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
	sleep(1);
	while(1)
	{
		for(i=0;i<100;i++)
		{
			pthread_mutex_lock(&mutex);
			distance_current = distance;
			pthread_mutex_unlock(&mutex);
			distance_diff = distance_current - distance_previous;
			printf("Distance = %0.2f cm \n",distance_current);
		
			rightNumber = i%10;
			leftNumber = i/10;
			
			tempBuffer[0] = numberBuffer[leftNumber][0];
			tempBuffer[1] = numberBuffer[leftNumber][1];
			tempBuffer[2] = numberBuffer[leftNumber][2];
			tempBuffer[3] = numberBuffer[leftNumber][3];

			tempBuffer[4] = numberBuffer[rightNumber][0];
			tempBuffer[5] = numberBuffer[rightNumber][1];
			tempBuffer[6] = numberBuffer[rightNumber][2];
			tempBuffer[7] = numberBuffer[rightNumber][3];

			spi_led_ioctl(fd,tempBuffer);
			
			sequenceBuffer[0] = 0;
			if(distance_current > 200)
			{
				sequenceBuffer[1] = 1000;
			}
			else if(distance_current < 2)
			{
				sequenceBuffer[1] = (int)(10 * 5);
			}
			else
			{
				sequenceBuffer[1] = (int)(distance_current * 5);
			}
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
			usleep(10000);
		}
	}
	//printf("thread_transmit_spi fd = %d\n",fd);
	close(fd);
	pthread_exit(0);
}

/***********************************************************************
* thread_transmit_spi_number - Thread Function to display distance 
* 		measured from sensor on the LED. The distance is displayed in
* 		centimeters from 0 to 99.
* @data: Thread Parameters
*
* Returns NULL
* 
* Description:  Thread Function to display distance 
* 		measured from sensor on the LED. The distance is displayed in
* 		centimeters from 0 to 99. Any distance above 99cm is displayed
* 		as 99cm on the LED.
***********************************************************************/
void *thread_transmit_spi_distance_number(void *data)
{
	int retValue,fd;
	int i,j,k;
	int leftNumber = 0, rightNumber = 0;
	ThreadParams *tparams = (ThreadParams*)data;
	double distance_previous = 0, distance_current = 0, distance_diff = 0;
	char numberBuffer[10][4] = {
		{0x7e, 0x81, 0x81, 0x7e},
		{0x84, 0x82, 0xff, 0x80},
		{0x61, 0x91, 0x91, 0x8e},
		{0x81, 0x99, 0x99, 0x7e},
		{0x0f, 0x08, 0x08, 0xff},
		{0x8e, 0x91, 0x91, 0x61},
		{0x7e, 0x89, 0x89, 0x71},
		{0x01, 0x01, 0x01, 0xfe},
		{0x6e, 0x91, 0x91, 0x6e},
		{0x8e, 0x91, 0x91, 0x7e},
		};
	char tempBuffer[8];
	unsigned int sequenceBuffer[4];
	//printf("thread_transmit_spi Start\n");

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
	
	while(1)
	{
		for(i=0;i<100;i++)
		{
			pthread_mutex_lock(&mutex);
			distance_current = distance;
			pthread_mutex_unlock(&mutex);
			distance_diff = distance_current - distance_previous;
			printf("Distance = %0.2f cm\n",distance_current);
		
			if(distance_current > 100)
			{
				rightNumber = 9;
				leftNumber = 9;
			}
			else
			{
				rightNumber = (int)distance_current%10;
				leftNumber = (int)distance_current/10;
			}
			
			tempBuffer[0] = numberBuffer[leftNumber][0];
			tempBuffer[1] = numberBuffer[leftNumber][1];
			tempBuffer[2] = numberBuffer[leftNumber][2];
			tempBuffer[3] = numberBuffer[leftNumber][3];

			tempBuffer[4] = numberBuffer[rightNumber][0];
			tempBuffer[5] = numberBuffer[rightNumber][1];
			tempBuffer[6] = numberBuffer[rightNumber][2];
			tempBuffer[7] = numberBuffer[rightNumber][3];

			spi_led_ioctl(fd,tempBuffer);
			
			sequenceBuffer[0] = 0;
			sequenceBuffer[1] = 1000;
			sequenceBuffer[2] = 0;
			sequenceBuffer[3] = 0;
			
			spi_led_write(fd, sequenceBuffer);
			usleep(10000);
		}
	}
	//printf("thread_transmit_spi fd = %d\n",fd);
	close(fd);
	pthread_exit(0);
}


/***********************************************************************
* thread_transmit_spi_user - Thread Function to display sequence of
* 	pattern. There is no distance control measurement here. It is just a	
* 	test program to check if input sequence is displayed correctly.
* @data: Thread Parameters
*
* Returns NULL
* 
* Description:  Thread Function to display sequence of
* 	pattern. There is no distance control measurement here. It is just a	
* 	test program to check if input sequence is displayed correctly.
***********************************************************************/
void *thread_transmit_spi_user(void *data)
{
	int retValue,fd;
	int i,j,k;
	ThreadParams *tparams = (ThreadParams*)data;
    char patternBuffer [10][8] = {
		{ 0x7C, 0x7E, 0x13, 0x13, 0x7E, 0x7C, 0x00, 0x00 }, // 'A'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x49, 0x7F, 0x36, 0x00 }, // 'B'
		{ 0x1C, 0x3E, 0x63, 0x41, 0x41, 0x63, 0x22, 0x00 }, // 'C'
		{ 0x41, 0x7F, 0x7F, 0x41, 0x63, 0x3E, 0x1C, 0x00 }, // 'D'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x5D, 0x41, 0x63, 0x00 }, // 'E'
		{ 0x41, 0x7F, 0x7F, 0x49, 0x1D, 0x01, 0x03, 0x00 }, // 'F'
		{ 0x1C, 0x3E, 0x63, 0x41, 0x51, 0x73, 0x72, 0x00 }, // 'G'
		{ 0x7F, 0x7F, 0x08, 0x08, 0x7F, 0x7F, 0x00, 0x00 }, // 'H'
		{ 0x00, 0x41, 0x7F, 0x7F, 0x41, 0x00, 0x00, 0x00 }, // 'I'
		{ 0x30, 0x70, 0x40, 0x41, 0x7F, 0x3F, 0x01, 0x00 }, // 'J'
	};
	unsigned int sequenceBuffer[20] = {0, 100, 1, 200, 3, 300, 4, 400, 5, 500, 6, 600, 7, 700, 8, 800, 0, 0};
	//printf("thread_transmit_spi Start\n");

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
	
	retValue = spi_led_ioctl(fd, patternBuffer, sizeof(patternBuffer));
	while(1)
	{
		retValue = spi_led_write(fd, sequenceBuffer, sizeof(sequenceBuffer));
		usleep(10000);
	}

	//printf("thread_transmit_spi fd = %d\n",fd);
	close(fd);
	pthread_exit(0);
}
/***********************************************************************
* thread_Ultrasonic_distance - Thread Function to measure the distance
* 			using the pulsewidth obtained from sensor.
* @data: Thread Parameters
*
* Returns NULL
* 
* Description:  Thread Function to measure the distance using the
* 			 pulsewidth obtained from sensor. Multiplying the pulsewidth
* 			with 0.017 gives the distance measured in cm.
***********************************************************************/
void *thread_Ultrasonic_distance(void *data)
{
	int fd;
	int pulseWidth;
	fd = open(PULSE_DEVICE_NAME, O_RDWR);
	while(1)
	{
		write_pulse(fd);
		pulseWidth = read_pulse(fd);
		pthread_mutex_lock(&mutex);
		distance = pulseWidth * 0.017;
		pthread_mutex_unlock(&mutex);
		usleep(100000);
	}
	close(fd);
	pthread_exit(0);
}

/***********************************************************************
* main - Main thread is used to used to create threads for distance and
* 		LED display.
* @argc: Parameters
* @argv: Parameters
* @envp: Parameters
*
* Returns 0
* 
* Description:  Main thread is used to used to create threads for 
* 		distance and LED display. Based on the user input 2 threads are
* 		created, one is for distance measurement from the sensor and 
* 		other is user input specific, which decides on the thread is 
* 		to display counter, or display dog, or display distance measured
* 		on the screen
***********************************************************************/
int main(int argc, char **argv, char **envp)
{
	int retValue, input;
	pthread_t thread_id_spi, thread_id_dist;
	ThreadParams *tp_spi, *tp_dist;
	
	printf("Enter \n1. To See dog controlled by sensor\n2. Number counter controlled by sensor\n3. Display distance on Sensor\n4. User Defined Pattern\n");
	scanf("%d",&input);
	
	/* Mutex Initialization*/
	pthread_mutex_init(&mutex, NULL);
	
	tp_spi = malloc(sizeof(ThreadParams));
	tp_spi -> threadId = 100;

	if (input == 2)
	{
		retValue = pthread_create(&thread_id_spi, NULL, &thread_transmit_spi_number, (void*)tp_spi);
	}
	else if (input == 3)
	{
		retValue = pthread_create(&thread_id_spi, NULL, &thread_transmit_spi_distance_number, (void*)tp_spi);
	}
	else if(input == 4)
	{
		retValue = pthread_create(&thread_id_spi, NULL, &thread_transmit_spi_user, (void*)tp_spi);
	}
	else
	{
		retValue = pthread_create(&thread_id_spi, NULL, &thread_transmit_spi_dog, (void*)tp_spi);
	}
		
		
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
	
	free(tp_spi);
	free(tp_dist);
	
	return 0;
} 


/***********************************************************************
* write_pulse - Function to send trigger pulse to sensor
* @fd: File Descriptor
*
* Returns negative errno, or else the number of success of trigger
* 
* Description: Function to send trigger pulse to sensor. It indirectly
* calls write method of the pulse.c to send a trigger pulse to sensor.
***********************************************************************/
int write_pulse(int fd)
{
	int retValue=0, count=0;
    char* writeBuffer;
	
	writeBuffer = (char *)malloc(10);
	while(1)
	{
		retValue = write(fd, writeBuffer, 10);
		if(retValue < 0)
		{
			//printf("Trigger Failure\n");
			//perror("PULSE Write ERROR is : ");
		}
		else
		{
			//printf("Trigger Successful\n");
			break;
		}
		usleep(100000);
	}
	free(writeBuffer);
	return retValue;
}

/***********************************************************************
* read_pulse - Function to read pulsewidth measured from sensor.
* @fd: File Descriptor
*
* Returns negative errno, or else the number of success of read.
* 
* Description: Function to read pulsewidth measured from sensor. This
* function makes a system call to read function of pulse.c
***********************************************************************/
int read_pulse(int fd)
{
	int retValue=0, count=0;
    unsigned int writeBuffer =0;
	while(1)
	{
		retValue = read(fd, &writeBuffer, sizeof(writeBuffer));
		
		if(retValue < 0)
		{
			//printf("Read Failure\n");
			//perror("PULSE Read ERROR is : ");
		}
		else
		{
			//printf("Read Successful\n");
			break;
		}
		usleep(100000);
	}
	return writeBuffer;
}


/***********************************************************************
* spi_led_write - Function to write a sequence of pattern onto LED.
* @fd: File Descriptor
* @sequenceBuffer: Order of pattern and time for each pattern to be 
* 					passed.
*
* Returns success or failure of the write.
* 
* Description: Function to write a sequence of pattern onto LED. This
* function makes a system call to write function of spi_led.c
***********************************************************************/
int spi_led_write(int fd, unsigned int sequenceBuffer[20])
{
	int retValue=0, count=0;
	while(1)
	{
		retValue = write(fd, sequenceBuffer, sizeof(sequenceBuffer));
		if(retValue < 0)
		{
			//printf("SPI LED Write Failure\n");
			//perror("SPI LED ERROR is : ");
		}
		else
		{
			//printf("SPI LED Write Successful\n");
			break;
		}
		usleep(100000);
	}
	return retValue;
}

/***********************************************************************
* spi_led_ioctl - Function to send pattern to buffer of driver.
* @fd: File Descriptor
* @patternBuffer: Entire pattern is passed to driver, so it can
* 					select the pattern and display based on write calls
* 					to driver.
*
* Returns success or failure of the ioctl.
* 
* Description: Function to send pattern to buffer of driver. This
* function makes a system call to ioctl function of spi_led.c
***********************************************************************/
int spi_led_ioctl(int fd, char patternBuffer[10][8])
{
	int retValue=0, count=0;
	while(1)
	{
		retValue = ioctl(fd, patternBuffer, sizeof(patternBuffer));
		if(retValue < 0)
		{
			printf("SPI LED IOCTL Failure\n");
		}
		else
		{
			//printf("spi_led_ioctl SPI LED IOCTL Successful\n");
			break;
		}
		usleep(100000);
	}
	return retValue;
}
