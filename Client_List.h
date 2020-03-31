#ifndef CLIENT_LIST_H
#define CLIENT_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>

//List struct which holds the new clients 

#define MAX_SIZE 50

struct client_list{ 

	char IP[INET_ADDRSTRLEN];
	int port;
	struct client_list* next;	
	
};

//The use of each function will be explained in the .c file

struct client_list* new_list(char* IP, int port);

int find_client(struct client_list* ct, char* str, int realport );

struct client_list* get_last(struct client_list* ct);

struct client_list* Add_Node(char* temp, int realport);

int get_count(struct client_list* ct, char* str, int realport );

void Removal(struct client_list* iroot, char* str, int realport);

void DestroyClientList(struct client_list* croot);


#endif