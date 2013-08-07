// Copyright (C) 2013  O. Alperen Elhan

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <linux/module.h>		/* Needed by all modules */
#include <linux/kernel.h>		/* Needed for KERN_INFO */
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/fs.h>  			/* everything */
#include <linux/types.h> 		/* size_t */
#include <linux/slab.h>			/* kmalloc */
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/ioctl.h>
#include <linux/semaphore.h>
#include <linux/timer.h>

/************ Defines **************/

#define MAX_QUEUE_LENGTH 3
#define MAX_FRAME_SIZE 1944
#define MAX_FPS 30
#define MIN_FPS 1
#define DEVICE_NAME "asciistreamer"
#define DEFAULT_FPS 5
#define SET_FPS_RATE _IO('o',1)
#define GET_FPS_RATE _IO('o',2)

/***********************************/


/********************** KERN_INFO ***********************/

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Osman Alperen Elhan");
MODULE_DESCRIPTION("An ASCII Movie Streamer Char Device");

/********************************************************/


/******************** Function Prototypes *******************/

/* File Operations Function Prototypes */
static int ascii_init(void);
static int ascii_open(struct inode *, struct file *);
static ssize_t ascii_read(struct file *, char *, size_t, loff_t *);
static ssize_t ascii_write(struct file *, const char *, size_t, loff_t *);
static long ascii_ioctl(struct file *, unsigned int, unsigned long );
static int ascii_release(struct inode *, struct file *);
static void ascii_exit(void);

/* Semaphore Operations Function Prototypes */
void init_semaphores(void);
void exit_semaphores(void);

/* Queue Operations Function Prototypes */
void queue_init(void);
void queue_exit(void);
void enqueue(char *);
char* dequeue(void);
void initEmptyArray(void);
void exitEmptyArray(void);

/* Timer Operations Function Prototypes */
void tick_tock(unsigned long);
void timer_init(void);
void timer_exit(void);

/********************************************************/


/***************** Variables and Structs ***************/

/* To hold frames */
static char** frameQueue;

/* Temprorary frame for enqueueing */
static char *arr;

/* Frame that readers use to read */
static char *currentFrame;

/* Empty frame to send when necessary */
static char *emptyArray;

/* Major number of the device */
static int major;

/* Count of writer threads */
static int writerDevice;

/* udev class */
static struct class *class_ascii;

/* our device */
static struct device *dev_ascii;

/* fps rate of the device */
static int fps = DEFAULT_FPS;

/* timer to send frames periodically */
static struct timer_list timer;

/* semaphore to handle consumer/producer problem */
static struct semaphore cpHandler;

/* semaphore to handle writers if queue is full */
static struct semaphore queueHandler;

/* semaphore to handle writers. There can be only one writer thread */
static struct semaphore wrHandler;

/* file operations of the device */
static struct file_operations fops = {
	.open = ascii_open,
	.read = ascii_read,
	.write = ascii_write,
	.unlocked_ioctl = ascii_ioctl,
	.release = ascii_release
};

/********************************************************/


/*********************************** File Operations **********************************/

static int ascii_init(void) {

	void *ptr_err;
	major = register_chrdev(0, DEVICE_NAME, &fops );

	if(major < 0){
		printk(KERN_INFO "Char device registering failed\n");
		return major;
	}
	printk(KERN_INFO "Device successfully created with major number: %d.\n", major);

	class_ascii = class_create(THIS_MODULE, DEVICE_NAME);

	if(IS_ERR(ptr_err = class_ascii)){
		printk(KERN_INFO "Creating class failed. Unregistering device.\n");
		unregister_chrdev(major, DEVICE_NAME);
		return PTR_ERR(ptr_err);
	}

	dev_ascii = device_create(class_ascii, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

	if(IS_ERR(ptr_err = dev_ascii)){
		printk(KERN_INFO "Creating Device failed. Unregistering device.\n");
		class_destroy(class_ascii);
		unregister_chrdev(major, DEVICE_NAME);
		return PTR_ERR(ptr_err);
	}

	init_semaphores();
	queue_init();
	initEmptyArray();
	timer_init();

	return 0;
}

static int ascii_open(struct inode *inode, struct file *file) {

	if((file->f_flags & O_ACCMODE) == O_RDWR )
		return -EINVAL;

	if((file->f_flags & O_ACCMODE) == O_WRONLY ){

		if(writerDevice > 0)
			return -EINVAL;

		writerDevice++;
	}

	if(!writerDevice)
		return -EINVAL;

	return 0;
}

static ssize_t ascii_read(struct file *file, char *buff, size_t count, loff_t *ppos) {

	if(!writerDevice && frameQueue[0] == NULL)
		return -EINVAL;

	if(count != MAX_FRAME_SIZE)
		return -1;

	if(copy_to_user(buff,currentFrame,MAX_FRAME_SIZE))
		return -EFAULT;

	return count;
}

static ssize_t ascii_write(struct file *file, const char *buff, size_t len, loff_t *off) {

	if(down_trylock(&wrHandler))
		return -EINVAL;

	down(&queueHandler);
	down(&cpHandler);

	if(len != MAX_FRAME_SIZE)
		return -1;

	arr = kmalloc(sizeof(char)*MAX_FRAME_SIZE, GFP_KERNEL);
	copy_from_user(arr, buff, MAX_FRAME_SIZE);
	enqueue(arr);

	up(&cpHandler);
	up(&wrHandler);

	return len;
}

static long ascii_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {

	int *p;

	switch(cmd){

		case GET_FPS_RATE:
			p = (int *) arg;
			*p = fps;

			break;

		case SET_FPS_RATE:
			if((file->f_flags & O_ACCMODE) == O_RDONLY)
				return -EFAULT;

			if(arg <= MAX_FPS && arg >= MIN_FPS)
				fps = (int) arg;

			break;
	}

	return 0;
}

static int ascii_release(struct inode *inode, struct file *file) {

	if((file->f_flags & O_ACCMODE) == O_WRONLY )
		writerDevice--;

	return 0;
}

static void ascii_exit(void) {

	timer_exit();
	exitEmptyArray();
	queue_exit();
	exit_semaphores();
	device_destroy(class_ascii, MKDEV(major, 0));
	class_destroy(class_ascii);

	printk(KERN_INFO "Device unregistering succeed\n");

	return unregister_chrdev(major, DEVICE_NAME);
}

/**************************************************************************************/


/************** Semaphore Operations **************/

void init_semaphores() {

	sema_init(&cpHandler , 1);
	sema_init(&queueHandler , MAX_QUEUE_LENGTH);
	sema_init(&wrHandler , 1);
}

void exit_semaphores() {

}

/*************************************************/


/****************** Timer Operations *******************/

void tick_tock( unsigned long data ) {

	char *tmp;

	down(&cpHandler);
	tmp = dequeue();

	if(tmp == NULL)
		currentFrame = emptyArray;
	else{
		currentFrame = tmp;
		up(&queueHandler);
	}

	mod_timer(&timer,jiffies+HZ/fps);
	up(&cpHandler);
}

void timer_init() {

	init_timer(&timer);
	timer.function = tick_tock;
	timer.data = 1;
	timer.expires = jiffies + HZ/fps;
	add_timer(&timer);
}

void timer_exit() {

	del_timer_sync(&timer);
}

/**********************************************************/


/********************** Queue Operations ******************/

void queue_init() {

	int i;
	frameQueue = kmalloc(sizeof(char*)*3, GFP_KERNEL);

	for (i = 0; i < 3; i++)
		frameQueue[i] = NULL;
}

void queue_exit() {

	kfree(frameQueue);
}

void enqueue(char *frame) {

	int i;

	for (i = 0; i < 3; ++i) {

		if(frameQueue[i] == NULL){
			frameQueue[i] = frame;
			return;
		}
	}
}


char* dequeue() {

	char* frame;
	frame = frameQueue[0];
	frameQueue[0] = frameQueue[1];
	frameQueue[1] = frameQueue[2];
	frameQueue[2] = NULL;

	return frame;
}


void initEmptyArray() {

	int i;
	emptyArray = kmalloc(sizeof(char)*MAX_FRAME_SIZE, GFP_KERNEL);

	for (i = 0; i < MAX_FRAME_SIZE; i++) {

		if(i%80 == 0 && i!=0)
			emptyArray[i] = '\n';
		else
			emptyArray[i] = ' ';
	}
}

void exitEmptyArray() {

	kfree(emptyArray);
}

/**********************************************************/


/* Initialize module */
module_init(ascii_init);
module_exit(ascii_exit);
