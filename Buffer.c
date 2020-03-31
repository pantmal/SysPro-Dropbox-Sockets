#include <stdio.h>
#include <sys/wait.h>	     
#include <sys/types.h>	     
#include <sys/socket.h>	     
#include <netinet/in.h>	     
#include <netdb.h>	         
#include <unistd.h>	         
#include <stdlib.h>	         
#include <ctype.h>	         
#include <signal.h>          
#include <string.h>
#include <arpa/inet.h>
#include <error.h>
#include <pthread.h>

#include "Buffer.h"


int isFull(struct buffer* bpool, int bsize){
    if( (bpool->start == bpool->end + 1) || (bpool->start == 0 && bpool->end == bsize-1)) {
		return 1;
	}
    return 0;
}

int isEmpty(struct buffer* bpool){
    if(bpool->start == -1){ 
		return 1;
	}
    return 0;
}

void place(struct buffer* bpool, struct buffer_item bp, int bsize) { //Placing an item in the buffer
	pthread_mutex_lock(&mtx);
	while (bpool->counter >= bsize) {
		printf("Found Buffer Full !! \n");
		pthread_cond_wait(&cond_nonfull, &mtx);
	}

	bpool->end = (bpool->end + 1) % bsize;

	bpool->buff_array[bpool->end].IP=malloc(strlen(bp.IP)+1);
	strcpy(bpool->buff_array[bpool->end].IP,bp.IP );
    bpool->buff_array[bpool->end].port=bp.port;
	

	if(bp.pathname!=NULL && bp.version!=-1){
		bpool->buff_array[bpool->end].pathname=malloc(strlen(bp.pathname)+1);
		strcpy(bpool->buff_array[bpool->end].pathname,bp.pathname);
		bpool->buff_array[bpool->end].version=bp.version;
		
	}

	bpool->counter++;
	pthread_mutex_unlock(&mtx);
}

struct buffer_item obtain(struct buffer* bpool, int bsize) {//Removing an item from the buffer
	struct buffer_item ret;
	
	pthread_mutex_lock(&mtx);
	while (bpool->counter <= 0) {
		printf("Found Buffer Empty !! \n");
		pthread_cond_wait(&cond_nonempty, &mtx);
	}

	ret = bpool->buff_array[bpool->start];

	bpool->buff_array[bpool->start].IP=NULL;
	free(bpool->buff_array[bpool->start].IP);
	bpool->buff_array[bpool->start].port=0;
	
	bpool->buff_array[bpool->start].pathname=NULL;
	free(bpool->buff_array[bpool->start].pathname);
	bpool->buff_array[bpool->start].version=-1;
	
	bpool->start = (bpool->start + 1) % bsize;

	bpool->counter--;

	pthread_mutex_unlock(&mtx);
	return ret;
}