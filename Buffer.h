#ifndef BUFFER_H
#define BUFFER_H

#include <stdio.h>
#include <sys/wait.h>	     /* sockets */
#include <sys/types.h>	     /* sockets */
#include <sys/socket.h>	     /* sockets */
#include <netinet/in.h>	     /* internet sockets */
#include <netdb.h>	         /* gethostbyaddr */
#include <unistd.h>	         /* fork */		
#include <stdlib.h>	         /* exit */
#include <ctype.h>	         /* toupper */
#include <signal.h>          /* signal */
#include <string.h>
#include <arpa/inet.h>
#include <error.h>
#include <pthread.h>

//Circular buffer used in the dropbox client application

struct buffer_item{//Item placed in the buffer
    char* IP;
    int port;
    char* pathname;
    int version;
};

struct buffer{//The buffer struct consists of an array of type buffer_items, a counter and its starting and ending posistions
    struct buffer_item* buff_array;
    int counter;
    int start;
    int end;
};

//Mutex and condition variables used when a thread needs to access the buffer
pthread_mutex_t mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;

//The use of each function will be explained in the .c file

int isFull(struct buffer* bpool, int bsize);

int isEmpty(struct buffer* bpool);

void place(struct buffer* bpool, struct buffer_item bp, int bsize);

struct buffer_item obtain(struct buffer* bpool, int bsize);



#endif