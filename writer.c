/*
 ============================================================================
 Name        : writer.c
 Author      : Osman Alpern Elhan
 Version     :
 Copyright   : GPLv2
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <unistd.h>

#define GET_FPS_RATE _IO('o',2)
#define SET_FPS_RATE _IO('o',1)

int main(int argc, char const *argv[]) {
	int a;
	a = open(argv[1],0666);
	char arr[1944];
	int i;
	int index =  open("/dev/asciistreamer", O_WRONLY);
	if(index == -1){
		printf("Cannot open device!");
		return EXIT_FAILURE;
	}
	ioctl(index,SET_FPS_RATE,15);
	for (i = 0; i < 1944; i++) {
		arr[i] = ' ';
	}
	int j=0;
	while((j=read(a, arr, 1944))>0){
		write(index, arr, 1944);
	}
	return 0;
}
