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
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#include "Client_List.h"
#include "Buffer.h"
#include "Functions.h"



int hash_func(char* user, int entries){   //Hash function in order to get file version
	
	  int t=0;
      int value=0;
      while(user[t]!='\0'){
        value=value+user[t];
        t++;
      }

      return value%entries;
	
}

char *strremove(char *str, const char *sub){ //This function is used to remove some substrings. 
    size_t len = strlen(sub);
    
    if (len > 0) {
        char *p = str;
        while ((p = strstr(p, sub)) != NULL) {
            memmove(p, p + len, strlen(p + len) + 1);
        }
    }

    return str;
}

int count_files(char* fpath){//Getting number of filepaths that will be sent in the GET_FILE_LIST message
    static int n=0;
    struct dirent* newent;
    DIR* new = opendir(fpath);  //Opening the directory from the current path we're on (fpath)
    

    while((newent = readdir(new)) != NULL){ //Opening the directory
        struct stat st;
        if(strcmp(newent->d_name, ".") == 0 || strcmp(newent->d_name, "..") == 0 || strcmp(newent->d_name, "Clients") == 0){ //Skipping ".." , "." strings and the Clients folder
            continue;
        }
        char checkpath[PATH_MAX]; //Checkpath string gets the path of the item we're on in order to check if it's dir or a regular file
        strcpy(checkpath,fpath); 
        strcat(checkpath,"/");
        strcat(checkpath,newent->d_name);
        
        struct stat path_stat;
        stat(checkpath, &path_stat);
        n++;
        

        if (fstatat(dirfd(new), newent->d_name, &st, 0) < 0){ //If it's not a directory, continue
            perror(newent->d_name);
            continue;
        }

        if (S_ISDIR(st.st_mode)){ //If the item is a directory
            
            char oldpath[PATH_MAX]; 
            strcpy(oldpath,fpath);
            strcat(oldpath,"/");
            strcat(oldpath,newent->d_name); //Getting the path of the directory we found
            
            count_files(oldpath); //Recursive call for the new dir we found
        } 



}
closedir(new); 

return n;
}

void send_files(char* fpath,int fd){//Sending files for GET_FILE_LIST message
    
    struct dirent* newent;
    DIR* new = opendir(fpath);  //Opening the directory from the current path we're on (fpath)
    

    while((newent = readdir(new)) != NULL){ //Opening the directory
        struct stat st;
        if(strcmp(newent->d_name, ".") == 0 || strcmp(newent->d_name, "..") == 0 || strcmp(newent->d_name, "Clients") == 0){ //Skipping ".." and "." strings
            continue;
        }
        char checkpath[PATH_MAX]; //Checkpath string gets the path of the item we're on in order to check if it's dir or a regular file
        strcpy(checkpath,fpath); 
        strcat(checkpath,"/");
        strcat(checkpath,newent->d_name);
        
        struct stat path_stat;
        stat(checkpath, &path_stat);
        if( S_ISREG(path_stat.st_mode)){ //Sending a regular file here

            
            int b2; //Sending bytes and name of the file
            b2=strlen(checkpath);
            int b2send = write(fd,&b2,sizeof(b2));
            int fsend = write(fd,checkpath,strlen(checkpath)+1);

            char pathh[PATH_MAX];
            strcpy(pathh,fpath);
            strcat(pathh,"/");
            strcat(pathh,newent->d_name); //Getting path of the file in order to get its size
            
            char* buffer;
            long length;
            FILE* fp=fopen(pathh,"r"); 
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

            int version=hash_func(buffer,length);//For the version of the file I decided to hash the contents of the file with file's size in bytes
            
            int vsend = write(fd,&version,sizeof(version));
            free(buffer);

        }
        

        if (fstatat(dirfd(new), newent->d_name, &st, 0) < 0){ //If it's not a directory, continue
            perror(newent->d_name);
            continue;
        }

        if (S_ISDIR(st.st_mode)){ //If the item is a directory
            
            int version=-1;

            int b2; //Sending bytes and name of the file
            b2=strlen(checkpath);
            int b2send = write(fd,&b2,sizeof(b2));
            int fsend = write(fd,checkpath,strlen(checkpath)+1);
            int vsend = write(fd,&version,sizeof(version));

            char oldpath[PATH_MAX]; 
            strcpy(oldpath,fpath);
            strcat(oldpath,"/");
            strcat(oldpath,newent->d_name); //Getting the path of the directory we found
            
            send_files(oldpath,fd); //Recursive call for the new dir we found
        } 



}
closedir(new); 


}