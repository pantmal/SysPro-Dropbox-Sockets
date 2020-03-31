#include <stdio.h>
#include <sys/types.h>	     
#include <sys/time.h>
#include <sys/socket.h>	     
#include <netinet/in.h>	     
#include <unistd.h>          
#include <netdb.h>	         
#include <stdlib.h>	         
#include <string.h>	         
#include <arpa/inet.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "Client_List.h"
#include "Buffer.h"
#include "Functions.h"

//Dropbox client application

//Some variables are in the global scope because they're needed in the signal handler and the worker thread function

struct client_list* croot=NULL;

char* dirname;
int cport=0;
int wthreads=0;
int bsize=0;
int sport=0;
char* sip;

int port, sock, i;
char buf[256];

struct sockaddr_in server;//Variables needed to set up the connections
struct sockaddr_in sender;
struct sockaddr_in client;
socklen_t senderlen;
struct sockaddr *serverptr = (struct sockaddr*)&server;
struct sockaddr *clientptr = (struct sockaddr*)&client;
struct sockaddr *senderptr = (struct sockaddr*)&sender;
struct hostent *rem;
struct sockaddr_in sa;
struct sockaddr_in ssa;
struct hostent *host_entry;
char hostbuffer[256];
int hostname;   
int scport=0;

struct buffer main_buffer;//Declaring the circular buffer

int fz;

char cwd[PATH_MAX];//Necessary paths
char clientspathsl[PATH_MAX];
//char inputpath[PATH_MAX];
char cwdninput[PATH_MAX];
char dirpath[PATH_MAX];
char pathofsender[PATH_MAX];

pthread_t* threads;//Threads and their respective numbers
int* thread_args;

void final_handler(int signum){ //Final Handler is used when a client receives the SIGINT signal
	
    if(croot!=NULL){
	    DestroyClientList(croot);//Destroying the list with the other clients
    }

    //Now connecting to the server in order to send the LOG_OFF message
    int sock3=0;
    if ((sock3 = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("socket");
    }
    if (connect(sock3, serverptr, sizeof(server)) < 0){
	   perror("connect");
    }


    char* log_off="LOG_OFF";
    int lfsize=strlen(log_off);
    
    if (write(sock3, &lfsize, sizeof(lfsize)) < 0){
        perror("write");
    }
    if (write(sock3, log_off, strlen(log_off)+1) < 0){
        perror("write");
    }
    if (write(sock3, &sa.sin_addr.s_addr, sizeof(sa.sin_addr.s_addr)) < 0){
        perror("write");
    }
    if (write(sock3, &scport, sizeof(scport)) < 0){
        perror("write");
    }
    close(sock3);
    
    printf("\nExiting the system.\n");

    //Finishing the threads with custom SIGUSR1 signal
    int i=0;
    int err=0;
    
    for (i=0 ; i<wthreads ; i++) {
		
        int status = pthread_kill( threads[i],SIGUSR1 );  
	}

    

    free(threads);
    free(thread_args);

    free(main_buffer.buff_array);

	exit(0);

}

void thread_handler(int signo){ //Signal handler used for the threads
    if(signo == SIGUSR1){
        pthread_exit((void *)1);     
    }
}



void* worker_thread(void *arguments);//Forward declaration of worker thread function


int main(int argc, char* argv[]){

    
    //Getting the parameters
    char* argm1=argv[1];
  	if(strcmp(argm1,"-d")==0){
        dirname=argv[2];
  	}
    char* argm3=argv[3];
    if(strcmp(argm3,"-p")==0){
        cport=atoi(argv[4]);
    }
    char* argm5=argv[5];
    if(strcmp(argm5,"-w")==0){
        wthreads=atoi(argv[6]);
    }
    char* argm7=argv[7];
    if(strcmp(argm7,"-b")==0){
        bsize=atoi(argv[8]);
    }
    char* argm9=argv[9];
    if(strcmp(argm9,"-sp")==0){
        sport=atoi(argv[10]);
    }
    char* argm11=argv[11];
    if(strcmp(argm11,"-sip")==0){
        sip=argv[12];
    }

    pthread_mutex_init(&mtx, 0);//Mutex and condition variables used when a thread needs to access the buffer
	pthread_cond_init(&cond_nonempty, 0);
	pthread_cond_init(&cond_nonfull, 0);

    //All the initial pathwork
  	if (getcwd(cwd, sizeof(cwd)) != NULL){ //Getting the current directory here, which will be needed throughout the program
       printf("Current working directory is: %s\n", cwd);
   	}else{
       perror("getcwd() error");
       return 1;
   	}

    char inputpath[PATH_MAX];
    strcpy(inputpath,cwd);//Path of cwd along with name of input directory
    strcat(inputpath,"/");
    strcat(inputpath,dirname);

    
    strcpy(cwdninput,cwd);//Same thing but with a "/" at the end 
    strcat(cwdninput,"/");
    strcat(cwdninput,dirname);
    strcat(cwdninput,"/");

    struct stat st = {0}; //Clients folder will store all the copies from the received files
    char clientspath[PATH_MAX];
    strcpy(clientspath,cwdninput);
    strcat(clientspath,"Clients");
    if (stat(clientspath, &st) == -1) {
    	mkdir(clientspath, 0777); 
	}

    
    strcpy(clientspathsl,clientspath);//Adding a "/" if need be
    strcat(clientspathsl,"/");


    main_buffer.buff_array=malloc(bsize * sizeof(struct buffer_item) );//Creating the circular buffer
    int f=0;
    for(f=0;f<bsize;f++){
        main_buffer.buff_array[f].IP=NULL;
        main_buffer.buff_array[f].pathname=NULL;
        main_buffer.buff_array[f].port=0;
        main_buffer.buff_array[f].version=-1;
    }
    main_buffer.counter=0;
    main_buffer.start=0;
    main_buffer.end=-1;

    fd_set active_fd_set, read_fd_set;

    //Getting IP and converting it along with the port to network format
    hostname = gethostname(hostbuffer, sizeof(hostbuffer)); 
    host_entry = gethostbyname(hostbuffer); 
    
    char *IPbuffer; 
     
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    printf("Client IP: %s \n", IPbuffer);

    inet_pton(AF_INET, IPbuffer, &(sa.sin_addr));
    scport=htons(cport);
    
    //Sock fd will be used to connect to the server and send LOG_ON and GET_CLIENTS messages, while msock will be used for the messages the client will recieve in the while 1 loop
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("socket");
    }

    inet_pton(AF_INET, sip, &(ssa.sin_addr));
    rem=gethostbyaddr((const  char*)&ssa.sin_addr,sizeof(ssa.sin_addr), AF_INET);
    server.sin_family = AF_INET;       //Internet domain 
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
    server.sin_port = htons(sport); //Server's port


    int msock=0;
    if ((msock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket");
    }
    client.sin_family = AF_INET;       //Internet domain 
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_port = htons(cport);      //The given port 

    // Bind socket to address 
    if (bind(msock, clientptr, sizeof(client)) < 0){
        perror("bind");
    }
    // Listen for connections 
    if (listen(msock, 7) < 0){ 
        perror("listen");
    }

    FD_ZERO (&active_fd_set);//Macros needed for the select function
    FD_SET (msock, &active_fd_set);


    //Socket work for sock and msock is over, now we"ll send to the server
    if (connect(sock, serverptr, sizeof(server)) < 0){
	   perror("connect");
    }
    

    char* log_on="LOG_ON";
    int lnsize=strlen(log_on);
    
    if (write(sock, &lnsize, sizeof(lnsize)) < 0){
        perror("write");
    }
    if (write(sock, log_on, strlen(log_on)+1) < 0){
        perror("write");
    }
    if (write(sock, &sa.sin_addr.s_addr, sizeof(sa.sin_addr.s_addr)) < 0){
        perror("write");
    }
    if (write(sock, &scport, sizeof(scport)) < 0){
        perror("write");
    }

    close(sock);
    
    //Sock2 used for the GET_CLIENTS message
    int sock2=0;
    if ((sock2 = socket(AF_INET, SOCK_STREAM, 0)) < 0){
    	perror("socket");
    }
    if (connect(sock2, serverptr, sizeof(server)) < 0){
	   perror("connect");
    }
    printf("Connecting ...\n");

    char* get_clients="GET_CLIENTS";
    int gcsize=strlen(get_clients);
    
    if (write(sock2, &gcsize, sizeof(gcsize)) < 0){
        perror("write");
    }
    if (write(sock2, get_clients, strlen(get_clients)+1) < 0){
        perror("write");
    }
    if (write(sock2, &sa.sin_addr.s_addr, sizeof(sa.sin_addr.s_addr)) < 0){
        perror("write");
    }
    if (write(sock2, &scport, sizeof(scport)) < 0){
        perror("write");
    }
    
    int n1=0;
    if(read(sock2,&n1,sizeof(n1))<0){ //Reading bytes of CLIENT_LIST message 			
		//print error
	}
    if(n1==11){
        
        char client__list[11];
        if(read(sock2,client__list,11+1)<0){ //CLIENT_LIST string					
		    //print error
		}
        if(strstr("CLIENT_LIST",client__list)!=NULL){
            int bigN=0;
            
            if (read(sock2,&bigN,sizeof(bigN))<0){
                //print error
            }
            while(1){
                
                int ip=0;
                if (read(sock2,&ip,sizeof(ip))<0){
                    //print error
                }

                if(ip==0){//If server sends the number zero, exit the loop
                    break;
                }
                
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
                printf("IP received: %s\n", str);

                int cport=0;
                if (read(sock2,&cport,sizeof(cport))<0){
                    //print error
                }
                int realport=0;
                realport=ntohs(cport);
                printf("Port received: %d \n",realport);

                if(croot==NULL){
                    croot=new_list(str,realport);
                    
                }else{
                    struct client_list* ctemp=croot;
      			    ctemp=get_last(ctemp); //Adding it to the end of the list

      			    struct client_list* new_node=Add_Node(str,realport);
      			    ctemp->next=new_node;
                }

            }
        }
    }
    
    
    close(sock2);
    
    //Allocating memory for number of threads that will be crated and their numbers
    threads=malloc(wthreads * sizeof(pthread_t));
    thread_args=malloc(wthreads * sizeof(int));
    int result_code;
    pthread_attr_t attr;

    //Each thread will get its number so we"ll know who is doing what
    int ii=0;
    for (ii = 0; ii < wthreads; ii++) {
        printf("Main thread creating working thread %d.\n", ii);
        
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        thread_args[ii] = ii;
        result_code = pthread_create(&threads[ii], &attr, worker_thread, &thread_args[ii]);
        pthread_attr_destroy ( &attr );
        assert(!result_code);
    }

    //Placing the IPs and ports that we got from the server in the buffer
    struct client_list* ctempfl=croot; 
    while(ctempfl!=NULL){

        struct buffer_item temp;
        temp.IP=ctempfl->IP;
        temp.port=ctempfl->port;
        temp.pathname=NULL;
        temp.version=-1;

        place(&main_buffer,temp,bsize);

        
        ctempfl=ctempfl->next;
    }
    pthread_cond_signal(&cond_nonempty);



    signal(SIGINT,final_handler);//Receiving signal in order to exit

    while(1){

        int mnewsock=0;
        read_fd_set = active_fd_set;
        if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0){//Select function used here in order to take care the first available file descriptor
          perror ("select");
          exit (EXIT_FAILURE);
        }
        int i=0; 
        for (i = 0; i < FD_SETSIZE; ++i){
            if (FD_ISSET (i, &read_fd_set)){
                if (i == msock){
                //Connection request on original socket. 
                FD_SET (mnewsock, &active_fd_set);
            
        
        if ((mnewsock = accept(msock, senderptr, &senderlen)) < 0){ //Accepting USER_ON and USER_OFF messages from server and GET_FILE_LIST and GET_FILE from other clients
            perror("accept");
        }

        
        int n=0;
        if(read(mnewsock, &n, sizeof(n)) != -1){  //Client reads bytes from received message and acts accordingly
        
        if(n==7){
            char msg[7];
            if(read(mnewsock,msg,7+1)<0){ //USER_ON string				
			    //print error
			}
            if(strstr("USER_ON",msg)!=NULL){

                printf("\nAdding new client \n");
                int ip=0;
                if (read(mnewsock,&ip,sizeof(ip))<0){
                    //print error
                }
                

                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
                printf("IP received: %s\n", str);

                int cport=0;
                if (read(mnewsock,&cport,sizeof(cport))<0){
                    //print error
                }
                int realport=0;
                realport=ntohs(cport);
                printf("Port received: %d \n",realport);


                //Placing the new client in the list and the buffer
                if(croot==NULL){
                    croot=new_list(str,realport);


                    struct buffer_item temp;
                    temp.IP=croot->IP;
                    temp.port=croot->port;
                    temp.pathname=NULL;
                    temp.version=-1;
                    
                    place(&main_buffer,temp,bsize);

                    pthread_cond_signal(&cond_nonempty); 

                    
                }else{
                    struct client_list* ctemp=croot;
      			    ctemp=get_last(ctemp); //Adding it to the end of the list

      			    struct client_list* new_node=Add_Node(str,realport);
      			    ctemp->next=new_node;

                    struct buffer_item temp;
                    temp.IP=new_node->IP;
                    temp.port=new_node->port;
                    temp.pathname=NULL;
                    temp.version=-1;  
                    
                    place(&main_buffer,temp,bsize);

                    pthread_cond_signal(&cond_nonempty); 

                }
            
            }
            close(mnewsock);
            continue;
        }

        if(n==8){
            char msg[8];
            if(read(mnewsock,msg,8+1)<0){ //USER_OFF string				
			    //print error
			}
            if(strstr("USER_OFF",msg)!=NULL){

                printf("\nRemoving client \n");

                int ip=0;
                if (read(mnewsock,&ip,sizeof(ip))<0){
                    //print error
                }
                
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ip, str, INET_ADDRSTRLEN);
                printf("IP received:  %s\n", str);

                int cport=0;
                if (read(mnewsock,&cport,sizeof(cport))<0){
                    //print error
                }
                int realport=0;
                realport=ntohs(cport);
                printf("Port received: %d \n",realport);

                if(croot->next==NULL){
                    DestroyClientList(croot);
                    croot=NULL;
                }else{
                    Removal(croot,str,realport);//Removing client from the list
                }
            
            }else if(strstr("GET_FILE",msg)!=NULL){//Code for get_file
                
                int psize=0;
                if (read(mnewsock,&psize,sizeof(psize))<0){
                    //print error
                }
                char filepath[psize];
                if (read(mnewsock,filepath,psize+1)<0){
                    //print error
                }

                int vread=0;
                if (read(mnewsock,&vread,sizeof(vread))<0){
                    //print error
                }

                char fullpath[PATH_MAX];
                strcpy(fullpath,inputpath);
                
                strcat(fullpath,filepath);
                //char* rem=(fullpath,)
                
                int exist=0;
                if( access( fullpath, F_OK ) != -1 ) {//Checking if file exists
                
                    exist=1;
                }

                if(exist==0){//If the requested file did not exist, send the FILE_NOT_FOUND string
                    printf("Main thread couldn't find file %s \n",fullpath);
                    char* not_found="FILE_NOT_FOUND";
                    int f2; 
                    f2=strlen(not_found);
                    int f2send = write(mnewsock,&f2,sizeof(f2));
                    int fnsend = write(mnewsock,not_found,strlen(not_found)+1);
                    
                }else{

                    char* buffer;
                    int length;
                    FILE* fp=fopen(fullpath,"r"); 
                    if (fp){
                        fseek(fp, 0, SEEK_END); //Seek to end of file
                        length = ftell(fp); //And get current file pointer, that's the size of the file
                        fseek(fp, 0, SEEK_SET);
                        buffer = malloc (length + 1);
                    }
                    if (buffer){
                        fread(buffer, 1, length, fp);
                    }
                    fclose(fp);
                    buffer[length] = '\0';
                    
		    
                    int version=hash_func(buffer,length);//For the version of the file I decided to hash the contents of the file with file's size in bytes
                    if(version==vread){//Same version here
                        printf("Main thread found %s file up to date \n",fullpath);
                        char* file_update="FILE_UP_TO_DATE";
                        int b2; 
                        b2=strlen(file_update);
                        int b2send = write(mnewsock,&b2,sizeof(b2));
                        int fsend = write(mnewsock,file_update,strlen(file_update)+1);
                    }else{//Version is different, so we"ll send the file

                        printf("Main thread sending file %s \n",fullpath);
                        char* file_size="FILE_SIZE";
                        int b2; 
                        b2=strlen(file_size);
                        int b2send = write(mnewsock,&b2,sizeof(b2));
                        int fsend = write(mnewsock,file_size,strlen(file_size)+1);

                        int vsend = write(mnewsock,&version,sizeof(version));
                        
                        int nsend = write(mnewsock,&length,sizeof(length));
                    
                        int ssend = write(mnewsock,buffer,length+1);
                    }

                    free(buffer);

                }

            } 
            close(mnewsock);
            continue;
        }

        if(n==13){//Code for get file list
            char msg[13];
            if(read(mnewsock,msg,13+1)<0){ //GET_FILE_LIST string		
			    //print error
			}
            if(strstr("GET_FILE_LIST",msg)!=NULL){

                printf("\nMain thread sending file list \n");
		
		
                int c2=strlen(inputpath);
                int c2send=write(mnewsock,&c2,sizeof(c2));
                int csend=write(mnewsock,inputpath,c2+1);//Sending path of input dir because it will be needed
                
                char* file_list="FILE_LIST";
                int b2; 
                b2=strlen(file_list);
                int b2send = write(mnewsock,&b2,sizeof(b2));
                int fsend = write(mnewsock,file_list,strlen(file_list)+1);
                
                int bigN=count_files(inputpath);//Getting how many paths will be sent

                int bigNsend = write(mnewsock,&bigN,sizeof(bigN));

                send_files(inputpath,mnewsock);//Sending paths

                //Sending -7 to exit the loop
                int mseven=-7;
                int msevenwrite=write(mnewsock,&mseven,sizeof(mseven));
                
            }

            close(mnewsock);
            continue;
        }

    }

    }//Inner if from select function
    

    }//Outer if from select function
    }//For loop from select function

    }//While 1 loop closes here
}

void* worker_thread(void *arguments){ //Worker thread
    
    
    signal(SIGUSR1,thread_handler);//Receiving SIGUSR1 signal from main thread

    int intex = *((int *)arguments);
    
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);           
                                                                                                     
    while(1){
        while(main_buffer.counter>0){
            
            struct buffer_item temp = obtain(&main_buffer,bsize);
            
            if(temp.pathname==NULL && temp.version==-1 ){ //Pathname and version have their default values so GET_FILE_LIST messge will be send
                
                
                if(find_client(croot,temp.IP,temp.port)==1){
                   
                    struct sockaddr_in ssa;//Finding and connecting to the client we spotted
                    struct sockaddr_in server;
                    struct hostent *rem;
                    struct sockaddr *serverptr = (struct sockaddr*)&server;
                    int sock=0;
                    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                        perror("socket");
                    }
                    inet_pton(AF_INET, temp.IP, &(ssa.sin_addr));
                    rem=gethostbyaddr((const  char*)&ssa.sin_addr,sizeof(ssa.sin_addr), AF_INET);
                    server.sin_family = AF_INET;       
                    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
                    server.sin_port = htons(temp.port);
                    if (connect(sock, serverptr, sizeof(server)) < 0){
                        perror("connect");
                    }
                    printf("%d: Connecting for GET_FILE_LIST ...\n",intex);

                    char* get_client_list="GET_FILE_LIST";
                    int ufsize=strlen(get_client_list);
                    
                    if (write(sock, &ufsize, sizeof(ufsize)) < 0){
                        perror("write");
                    }
                    if (write(sock, get_client_list, strlen(get_client_list)+1) < 0){
                        perror("write");
                    }

                    int fz=0;
                    if(read(sock, &fz, sizeof(fz)) != -1){ //Number of paths
                                //print error
                    }

                    char pathofsender[fz];
                    if(read(sock, pathofsender, fz+1) != -1){//Reading the name of sender's input dir path 
                        //print error
                    }
                    
                    int fn=0;

                    
                    if(read(sock, &fn, sizeof(fn)) != -1){ //Bytes of FILE_LIST message
                        //print error
                    }


                    if (fn==9){
                        
                        char flist[9];
                        if(read(sock,flist,9+1)<0){ //FILE_LIST string				
                            //print error
                        }
                        if(strstr("FILE_LIST",flist)!=NULL){
                            int bign=0;
                            
                            
                            strcpy(dirpath,clientspathsl);//Creating folder where the copies will be stored
                            
                            strcat(dirpath,temp.IP);
                            strcat(dirpath,"_");
                            char str[12];
                            sprintf(str, "%d", temp.port);
                            strcat(dirpath,str);
                            struct stat st = {0};
                            if (stat(dirpath, &st) == -1) {
    	                        mkdir(dirpath, 0777); 
	                        }
                            

                            if(read(sock, &bign, sizeof(bign)) != -1){ //Number of paths
                                //print error
                            }

                            char inputpaths[bign][fz+1];
                            int bs=0;
                            for(bs=0;bs<bign;bs++){
                                strcpy(inputpaths[bs],pathofsender);
                            }


                            int bss=0;
                            while(1){
                                int number=0;
                                if(read(sock, &number, sizeof(number)) != -1){
                                    //print error
                                }
                                if (number==-7){//Exiting the loop on -7 
                                    break;
                                }
                                char pathr[number];
                                if(read(sock,pathr,number+1)<0){ //Pathname is read here				
                                    //print error
                                }

                                int version=0;
                                if(read(sock, &version, sizeof(version)) != -1){//And version here
                                   //print error
                                }
                                if(version==-1){ // -1 version is set for directories. For them, we simply create the respective directories where files will be stored later on in the GET_FILE message
                                
                                    
                                    char newdir[PATH_MAX];
                                    
                                    
                                    char* remd=strremove(pathr,inputpaths[bss]);

                                    
                                    strcpy(newdir,dirpath);
                                    strcat(newdir,remd);
                                    struct stat st = {0};
                                    printf("%d: got new dir %s \n",intex,remd);
                                    if (stat(newdir, &st) == -1) {
                                        mkdir(newdir, 0777); 
                                    }


                                }else{//If we receive a regular file we place it in the buffer
                                
                                    
                                    char* remd=strremove(pathr,inputpaths[bss]);

                                    struct buffer_item btemp;
                                    btemp.IP=temp.IP;
                                    btemp.port=temp.port;
                                    btemp.pathname=remd;
                                    btemp.version=version;

                                    printf("%d: got new file %s \n",intex,remd);

                                    place(&main_buffer,btemp,bsize);

                                    pthread_cond_signal(&cond_nonempty);

                                }

                                bss++;

                            }
                        }
                    }


                    close(sock);
                }

                

            }else if(temp.pathname!=NULL && temp.version!=-1 && temp.IP!=NULL && temp.port!=0){ //GET_FILE message work here
                if(find_client(croot,temp.IP,temp.port)==1){

                    struct sockaddr_in ssa;//Finding and connecting to the client we spotted
                    struct sockaddr_in server;
                    struct hostent *rem;
                    struct sockaddr *serverptr = (struct sockaddr*)&server;
                    int sock=0;
                    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                        perror("socket");
                    }
                    inet_pton(AF_INET, temp.IP, &(ssa.sin_addr));
                    rem=gethostbyaddr((const  char*)&ssa.sin_addr,sizeof(ssa.sin_addr), AF_INET);
                    server.sin_family = AF_INET;       
                    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);
                    server.sin_port = htons(temp.port);
                    if (connect(sock, serverptr, sizeof(server)) < 0){
                        perror("connect");
                    }
                    printf("%d: Connecting for GET_FILE ...\n",intex);



                    char filepath[PATH_MAX];
                    strcpy(filepath,clientspathsl);

                    strcat(filepath,temp.IP);
                    strcat(filepath,"_");
                    char str[12];
                    sprintf(str, "%d", temp.port);
                    strcat(filepath,str);
                    
                    strcat(filepath,temp.pathname);
                    int exist=0;
                    if( access( filepath, F_OK ) != -1 ) { //Checking if file exists
                    
                        exist=1;
                    }
                    if(exist==0){
                        printf("%d: File %s doesn't exist \n",intex,filepath);
                        
                        int version=-1; //Setting default version value in order to get the requested file
                        char* get_file="GET_FILE";
                        int ufsize=strlen(get_file);
                        
                        if (write(sock, &ufsize, sizeof(ufsize)) < 0){
                            perror("write");
                        }
                        if (write(sock, get_file, strlen(get_file)+1) < 0){ //GET_FILE string
                            perror("write");
                        }

                        int psize=strlen(temp.pathname);
                        if (write(sock, &psize, sizeof(psize)) < 0){
                            perror("write");
                        }
                        if (write(sock, temp.pathname, strlen(temp.pathname)+1) < 0){//Pathname
                            perror("write");
                        }

                        if (write(sock, &version, sizeof(version)) < 0){//Version
                            perror("write");
                        }

                        int fn=0;
                        
                        if(read(sock, &fn, sizeof(fn)) != -1){
                            //print error
                        }

                        if(fn==14){ //Requested file was not found
                            char fsize[14];
                            if(read(sock,fsize,14+1)<0){ 
                                //print error
                            }
                            if(strstr("FILE_NOT_FOUND",fsize)!=NULL){
                                printf("%d: File %s not found \n",intex,filepath);
                            }
                        }

                        if(fn==9){//Otherwise we get the file
                            char fsize[9];
                            if(read(sock,fsize,9+1)<0){ //FILE_SIZE string				
                                //print error
                            }
                            if(strstr("FILE_SIZE",fsize)!=NULL){

                                int vread=0;
                                int smalln=0;

                                if(read(sock, &vread, sizeof(vread)) != -1){ //Version
                                    //print error
                                }
                                
                                if(read(sock, &smalln, sizeof(smalln)) != -1){ //Number of bytes
                                    //print error
                                }
                                
                                char stringr[smalln];
                                if(read(sock,stringr,smalln+1)<0){ //The content					
                                    //print error
                                }
                                
                                //Writing the content to copied file
                                FILE* fp=fopen(filepath,"w");
                                fprintf(fp,"%s", stringr );
                                fclose(fp);

                                printf("%d: File %s received \n",intex,filepath);

                            }
                        }


                    }else{ //If file exists get its version and send it to the other client for him to do the comparison
                        printf("%d: File %s exists \n",intex,filepath);
                        
                        char* buffer;
                        int length;
                        
                        FILE* fp=fopen(filepath,"r"); 
                        if (fp){
                            fseek(fp, 0, SEEK_END); //Seek to end of file
                            length = ftell(fp); //And get current file pointer, that's the size of the file
                            fseek(fp, 0, SEEK_SET);
                            buffer = malloc (length + 1);
                        }
                        if (buffer){
                            fread (buffer, 1, length, fp);
                        }
                        fclose(fp);
                        buffer[length] = '\0';
                        
                        int version=hash_func(buffer,length);
                        char* get_file="GET_FILE";
                        int ufsize=strlen(get_file);
                        
                        if (write(sock, &ufsize, sizeof(ufsize)) < 0){
                            perror("write");
                        }
                        if (write(sock, get_file, strlen(get_file)+1) < 0){//GET_FILE string
                            perror("write");
                        }

                        int psize=strlen(temp.pathname);
                        if (write(sock, &psize, sizeof(psize)) < 0){
                            perror("write");
                        }
                        if (write(sock, temp.pathname, strlen(temp.pathname)+1) < 0){//Pathname
                            perror("write");
                        }
                        
                        if (write(sock, &version, sizeof(version)) < 0){ //Version
                            perror("write");
                        }

                        int fn=0;

                        if(read(sock, &fn, sizeof(fn)) != -1){
                            //print error
                        }

                        if(fn==14){ //Requested file was not found
                            char fsize[14];
                            if(read(sock,fsize,14+1)<0){ 
                                //print error
                            }
                            if(strstr("FILE_NOT_FOUND",fsize)!=NULL){
                                printf("%d File %s not found \n",intex,filepath);
                            }
                        }

                        if(fn==9){ //If we receive the FILE_SIZE string this means the file changed
                            printf("%d: We've got a change here! \n",intex);
                            char fsize[9];
                            if(read(sock,fsize,9+1)<0){ //FILE_SIZE string					
                                //print error
                            }
                            if(strstr("FILE_SIZE",fsize)!=NULL){
                                int vread=0;
                                int smalln=0;

                                if(read(sock, &vread, sizeof(vread)) != -1){//Version
                                    //print error
                                }

                                if(read(sock, &smalln, sizeof(smalln)) != -1){//Number of bytes
                                    //print error
                                }

                                char stringr[smalln];
                                if(read(sock,stringr,smalln+1)<0){ //The content
                                    //print error
                                }

                                //Writing the content to copied file
                                FILE* fp=fopen(filepath,"w");
                                fprintf(fp,"%s", stringr );
                                fclose(fp);

                                printf("%d: File %s received \n",intex,filepath);

                            }
                        }

                        char filepath2[PATH_MAX];
                        strcpy(filepath2,filepath);

                        if(fn==15){ //Otherwise no changed happened so we receive the FILE_UP_TO_DATE message
                            
                            char flist[15];
                            if(read(sock,flist,15+1)<0){ //File name					
                                //print error
                            }
                            if(strstr("FILE_UP_TO_DATE",flist)!=NULL){
                                printf("%d: File %s is up to date! \n",intex,filepath2);
                            }
                        }

                        free(buffer);
                    }
                close(sock);
            }

            }
            
            pthread_cond_signal(&cond_nonfull);
        }
    }
}