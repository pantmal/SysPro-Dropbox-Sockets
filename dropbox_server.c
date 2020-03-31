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

#include "Client_List.h"

//Dropbox server application

struct client_list* croot=NULL; //Client list

void final_handler(int signum){ //Signal handler deletes the clients still kept from the server
    if(croot!=NULL){
        DestroyClientList(croot);
    }

    exit(0);
}

int main(int argc, char* argv[]){

    int serv_port; //Server port given by the user
    fd_set active_fd_set, read_fd_set; //Variables needed for the select function

    //Getting server's IP here so it can be used from the client application
    char hostbuffer[256]; 
    char *IPbuffer; 
    struct hostent *host_entry; 
    int hostname; 
  
    //Retrieve hostname 
    hostname = gethostname(hostbuffer, sizeof(hostbuffer)); 
    
    //Retrieve host information 
    host_entry = gethostbyname(hostbuffer); 
  
    //Converting the internet network address into ASCII string 
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])); 

    printf("Server IP: %s \n", IPbuffer);

    //Getting the parameter
    char* argm1=argv[1];
  	if(strcmp(argm1,"-p")==0){
        serv_port=atoi(argv[2]);
  	}

    struct sockaddr_in server, client;//Variables needed to set up the connections
    socklen_t clientlen;

    struct sockaddr *serverptr=(struct sockaddr *)&server;//For bind function
    struct sockaddr *clientptr=(struct sockaddr *)&client;//For accept function 
    struct hostent *rem;

    //Creating socket 
    int sock=0;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
    }
    server.sin_family = AF_INET;//Internet domain 
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(serv_port);//The given port 

    //Bind socket to address
    if (bind(sock, serverptr, sizeof(server)) < 0){
        perror("bind");
    }
    //Listen for connections 
    if (listen(sock, 7) < 0){ 
        perror("listen");
    }

    FD_ZERO(&active_fd_set); //Macros needed for the select function
    FD_SET(sock, &active_fd_set);

    signal(SIGINT,final_handler);//Receiving signal in order to exit
    while(1){
    
        int newsock=0; 

        read_fd_set = active_fd_set;
        if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0){ //Select function used here in order to take care the first available file descriptor
          perror("select");
          exit(EXIT_FAILURE);
        }

        int i=0; 
        for (i = 0; i < FD_SETSIZE; ++i){
            if (FD_ISSET (i, &read_fd_set)){
                if (i == sock){
                //Connection request on original socket. 
                FD_SET (newsock, &active_fd_set);

        if ((newsock = accept(sock, clientptr, &clientlen)) < 0){ //Accept sets up connection with client who enters the system
            perror("accept");
        }

    
        printf("\nAccepted connection\n");
        int n=0;
        if(read(newsock, &n, sizeof(n)) != -1){ //Server read number of bytes from each message, (i.e. 7 from LOG_OFF and acts accordingly)  
            if(n==7){
                char log_off[7];
                if(read(newsock,log_off,7+1)<0){ //LOG_OFF string				
				    printf("Error in read \n");
			    }
                if(strstr("LOG_OFF",log_off)!=NULL){
                    
                    int ip=0;
                    if (read(newsock,&ip,sizeof(ip))<0){ //Reading the IP in binary
                        printf("Error in read \n");
                    }
                    //Converting the IP to string format
                    char str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
                    printf("IP received: %s\n", str);

                    int cport=0;
                    if (read(newsock,&cport,sizeof(cport))<0){ //Reading the port in binary
                        printf("Error in read \n");
                    }
                    int realport=0;
                    realport=ntohs(cport); //Converting the port in binary format
                    printf("Port received: %d \n",realport);

                    printf("Logging off the user %s %d \n",str,realport);
                
                if(find_client(croot,str,realport)==1){ //Removing the client from the server's list

                    if(croot->next==NULL){ //If it's only one user, just destroy the list
                        DestroyClientList(croot);
                        croot=NULL;
                    }else{

                    Removal(croot,str,realport);//Otherwise remove a single node from the list
                    
                    struct client_list* ctempu=croot;
                    while(ctempu!=NULL){ //Now sending to the other clients the user who logged off 
                        
                        struct sockaddr_in ssa;//Variables needed to set up the connection with each client
                        struct sockaddr_in server;
                        struct hostent *rem;
                        struct sockaddr *serverptr = (struct sockaddr*)&server;
                        int sock=0;//Creating the socket
                        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                            perror("socket");
                        }
                        inet_pton(AF_INET, ctempu->IP, &(ssa.sin_addr));//IP from the client list is turned to network format
                        rem=gethostbyaddr((const  char*)&ssa.sin_addr,sizeof(ssa.sin_addr), AF_INET);
                        server.sin_family = AF_INET;// Internet domain 
                        memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
                        server.sin_port = htons(ctempu->port);//Same work for the port
                        if (connect(sock, serverptr, sizeof(server)) < 0){
                            perror("connect");
                        }
                        

                        //Sending the "USER_OFF" message, first its bytes and then the actual message
                        char* user_off="USER_OFF";
                        int ufsize=strlen(user_off);
                        
                        if (write(sock, &ufsize, sizeof(ufsize)) < 0){ 
                            perror("write");
                        }
                        if (write(sock, user_off, strlen(user_off)+1) < 0){
                            perror("write");
                        }
                        
                        //Now sending the IP and port of the client who left the system
                        struct sockaddr_in sa;
                        inet_pton(AF_INET, str, &(sa.sin_addr));
                        if (write(sock, &sa.sin_addr.s_addr, sizeof(sa.sin_addr.s_addr)) < 0){
                            perror("write");
                        }
                        int sendport=htons(realport);
                        if (write(sock, &sendport, sizeof(sendport)) < 0){
                            perror("write");
                        }

                        
                        close(sock);
                        ctempu=ctempu->next;
                
                    }

                    }

                }else{
                    //In case the client who left was never in the system
                    printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST \n");
                }
                
                }

                close(newsock);
                continue;
                
            }

    	    if(n==6){
                char log_on[6];
                if(read(newsock,log_on,6+1)<0){ //LOG_ON string			
				    printf("Error in read \n");
			    }
                
                if(strstr("LOG_ON",log_on)!=NULL){
                    int ip=0;
                    if (read(newsock,&ip,sizeof(ip))<0){ //Reading the IP in binary
                        printf("Error in read \n");
                    }
                    //Converting the IP to string format
                    char str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
                    printf("IP received: %s\n", str);

                    int cport=0;
                    if (read(newsock,&cport,sizeof(cport))<0){//Reading the port in binary 
                        printf("Error in read \n");
                    }
                    int realport=0;
                    realport=ntohs(cport);//Converting the port in binary format
                    printf("Port received: %d \n",realport);
                    
                    
                    printf("Successful log on from %s %d \n",str,realport);

                    if(croot==NULL){ //Creating the client list, if it doesn't exist
                        croot=new_list(str,realport);
                        
                    }else{ //Otherwise send to the other users who just logged on and add him to the server's list
                        if(find_client(croot,str,realport)==0){
                            struct client_list* ctempu=croot;
                            while(ctempu!=NULL){
                                struct sockaddr_in ssa;//Variables needed to set up the connection with each client
                                struct sockaddr_in server;
                                struct hostent *rem;
                                struct sockaddr *serverptr = (struct sockaddr*)&server;
                                int sock=0;//Creating the socket
                                if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	                            perror("socket");
                                }
                                inet_pton(AF_INET, ctempu->IP, &(ssa.sin_addr));//IP from the client list is turned to network format
                                rem=gethostbyaddr((const  char*)&ssa.sin_addr,sizeof(ssa.sin_addr), AF_INET);
                                server.sin_family = AF_INET; // Internet domain
                                memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
                                server.sin_port = htons(ctempu->port);//Same work for the port
                                if (connect(sock, serverptr, sizeof(server)) < 0){
                                    perror("connect");
                                }
                                

                                //Sending the "USER_OΝ" message, first its bytes and then the actual message
                                char* user_on="USER_ON";
                                int unsize=strlen(user_on);
                                if (write(sock, &unsize, sizeof(unsize)) < 0){
                                    perror("write");
                                }
                                if (write(sock, user_on, strlen(user_on)+1) < 0){
                                    perror("write");
                                }
                                
                                //Now sending the IP and port of the client who left the system
                                struct sockaddr_in sa;
                                inet_pton(AF_INET, str, &(sa.sin_addr));
                                if (write(sock, &sa.sin_addr.s_addr, sizeof(sa.sin_addr.s_addr)) < 0){
                                    perror("write");
                                }
                                int sendport=htons(realport);
                                if (write(sock, &sendport, sizeof(sendport)) < 0){
                                    perror("write");
                                }

                                close(sock);
                                ctempu=ctempu->next;
                            }


                            struct client_list* ctemp=croot;
      			            ctemp=get_last(ctemp); //Adding it to the end of the list

      			            struct client_list* new_node=Add_Node(str,realport);
      			            ctemp->next=new_node;

                            

                        }
                        
                    }

                }
                
                close(newsock);
                continue;
            }

            if(n==11){
                char get_clients[11];
                if(read(newsock,get_clients,11+1)<0){ //GET_CLIENTS string				
				    printf("Error in read \n");
			    }
                if(strstr("GET_CLIENTS",get_clients)!=NULL){
                    
                    //I decided to send the IP and port of the client so the server will know not send back the IP/port of the client who sent the GET_CLIENTS request

                    int ip=0;
                    if (read(newsock,&ip,sizeof(ip))<0){//Reading the IP in binary
                        printf("Error in read \n");
                    }
                    //Converting the IP to string format
                    char str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
                    printf("IP received: %s\n", str);

                    int cport=0;
                    if (read(newsock,&cport,sizeof(cport))<0){//Reading the port in binary
                        printf("Error in read \n");
                    }
                    int realport=0;
                    realport=ntohs(cport);//Converting the port in binary format
                    printf("Port received: %d \n",realport);

                    struct client_list* ctempu=croot;
                    int bigN=get_count(croot,str,realport);//Getting how many clients will be sent
                    if (bigN+1>1){

                        char* client__list="CLIENT_LIST";
                        int clsize=strlen(client__list);
                        
                        if (write(newsock, &clsize, sizeof(clsize)) < 0){//Sending bytes of CLIENT_LIST string
                            perror("write");
                        }
                        if (write(newsock, client__list, strlen(client__list)+1) < 0){ //Sending the CLIENT_LIST string
                            perror("write");
                        }
                        if (write(newsock, &bigN, sizeof(bigN)) < 0){ //Sending N number of clients
                            perror("write");
                        }
                        while(ctempu!=NULL){

                            if( (strcmp(ctempu->IP,str)==0) && (ctempu->port == realport) ){ //Skip the client who sent the request
                                ctempu=ctempu->next;
                                continue;   
                            }

                            //Sending the IP and port of the clients in the list
                            struct sockaddr_in tsa;
                            inet_pton(AF_INET, ctempu->IP, &(tsa.sin_addr));
                            if (write(newsock, &tsa.sin_addr.s_addr , sizeof(tsa.sin_addr.s_addr)) < 0){
                                perror("write");
                            }
                            int tport=htons(ctempu->port);
                            if (write(newsock, &tport, sizeof(tport)) < 0){
                                perror("write");
                            }

                            
                            ctempu=ctempu->next;
                        }

                        int zero=0;
                        if (write(newsock, &zero, sizeof(zero)) < 0){//Number zero finishes the sequence of sending clients
                            perror("write");
                        }

                    }

                    printf("Got clients message received succesfully \n");
                }

                close(newsock);
                continue;
                
            }

            
        }
        

    }//Inner if from select function
    }//Outer if from select function
    }//For loop from select function


    }//While 1 loop closes here
    


}