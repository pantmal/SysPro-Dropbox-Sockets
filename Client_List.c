#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Client_List.h"

struct client_list* new_list(char* IP, int port){ //Constructor function

	struct client_list* new=malloc(sizeof(struct client_list));
	strcpy(new->IP,IP);
    new->port=port;
    new->next=NULL;
    
	return (new);

}

int find_client(struct client_list* ct, char* str, int realport ){//Finding a client in the list
    
    int found=0;
    if(ct==NULL){
        printf("List is empty...\n");
    }
    while(ct != NULL){
        
        if( (strcmp(ct->IP,str)==0) && (ct->port == realport) ){
            found=1;
        }
        ct=ct->next;
    }

    return found;

}


struct client_list* get_last(struct client_list* ct){ //Getting the last node of the list

		while(ct->next != NULL){
          ct = ct->next;
        }

        return ct;
}

struct client_list* Add_Node(char* temp,int port){ //Adding a new node in the list
	
	struct client_list* new_node=malloc(sizeof(struct client_list));
    strcpy(new_node->IP,temp);
    new_node->port=port;
    new_node->next=NULL;

    return new_node;
      			
}

int get_count(struct client_list* ct, char* str, int realport ){//Getting how many clients will be send in the GET_CLIENTS message, excluding the client who sent the request
    int counter=0;
    while(ct != NULL){
        if( (strcmp(ct->IP,str)==0) && (ct->port == realport) ){ //Skip the client who sent the request
           ct=ct->next;
           continue; 
        }
        counter++;
        ct=ct->next;
    }

    return counter;
}


void Removal(struct client_list* iroot, char* str, int realport){ //Removing a single node from the list

	struct client_list* temp=iroot;
	struct client_list* prev=NULL;

	
	while(temp->next!=NULL ){
		if(strcmp(temp->IP,str)==0 && realport==temp->port ){
            break;
        }
        prev=temp;
		temp=temp->next;
        
	}
	
	if(strcmp(temp->IP,str)==0 && realport==temp->port ){
		
		if(prev==NULL){
			struct client_list* tnext=temp->next;
			free(temp);
			iroot=tnext;
		}else{
			struct client_list* tnext=temp->next;
			free(temp);
			prev->next=tnext;
			
		}
		
	}
	
}

void DestroyClientList(struct client_list* croot){ //Destructor function


	  struct client_list* itemp2=croot; 
      struct client_list* icurr=itemp2;
      struct client_list* inext;

      while(icurr !=NULL){

        inext=icurr->next;
        free(icurr);

        icurr=inext;

    }

    croot=NULL;
}

 