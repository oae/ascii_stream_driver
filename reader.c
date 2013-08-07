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
	char tmp[1944];
	int i;
	int index =  open("/dev/asciistreamer", O_RDONLY);
	if(index == -1){
		printf("Cannot open device!");
		return EXIT_FAILURE;
	}
	int result;
	while((result = read(index, tmp, 1944)) > 0){
		system("clear");
		//printf("%s",tmp);
		for (i = 0; i < 1944; i++) {
			printf("%c",tmp[i]);
		}
	}
	return 0;
}
