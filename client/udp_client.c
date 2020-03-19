/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/stat.h>
#include <errno.h>

#define BUFSIZE 1024
#define cypherkey 'S'
#define sendrecvflag 0
#define ACK "ACK"

extern int errno;
/* clear buffer */
void clearBuffer(char* b){
    for(int i = 0; i < BUFSIZE; i++){
        b[i] = '\0';
    }
}



/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);

    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    
     
    serverlen = sizeof(serveraddr);
    /* get command from user*/
    char user_command[BUFSIZE];
    char file_path[BUFSIZE];
    bzero(file_path, BUFSIZE);
    while(1){
        bzero(user_command, BUFSIZE);
        const char ch = '\n';
        printf("Please enter a command: ");
        fgets(user_command, BUFSIZE, stdin);
        char *pos; //to remove newline
        if((pos = strchr(user_command, ch)) != NULL){
            *pos = '\0';
        }       
        printf("command is: %s\n", user_command);
        strcpy(buf, user_command);
        char *command = strtok(user_command, " ");
        //command now contains the first word the user entered
       	//printf("command token is %s\n", command);
        if(strncmp(command, "get", 3) == 0){
            //if the command is get, create char fname and start it with ./,  assume file is in current dir
            char fname[BUFSIZE];
            strcpy(fname, "./");
            command = strtok(NULL, " ");
            //command now contains the second word the user entered, which should be the filename
	    if(command == NULL){
                printf("ERROR: Must enter file name\n");
                continue;
            }
            strcat(fname, command);
            printf("sending command %s and will write to path >%s< \n", buf, fname);
            n = 0;
            int c = 0;
            //send filename to sever
	    while( n < strlen(buf)){
                c = sendto(sockfd, buf+n, strlen(buf)-n, 0, (struct sockaddr *)&serveraddr, serverlen);
                //printf("sent %d\n", c);
                if(c < 0){
                    error("ERROR in sendto");
                    exit(1);
                }
                n += c;
            }
	    //command sent successfully
            printf("command sent \n");
            n = 1;
            c = 1;
            //write to different dir to prevent overwriting if on same machine
            FILE *fptr;
            if((fptr = fopen(fname, "wb")) == NULL){
                printf("ERROR opening file");
                exit(1);
            }
	    //file opened
            printf("receive file open, getting file size\n");
            clearBuffer(buf);
            //receive file data from server
	    c = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
            printf("string received: %s\n", buf);
            long bytes_to_write = strtol(buf, NULL, 10);
            printf("file size is: %ld\n", bytes_to_write);
            long total_received = 0;
            int num_packets = 0;
            //copy data from buffer to local file
	    while(bytes_to_write >0){
                clearBuffer(buf);
                printf("reveiving...\n");
                  
                c = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                printf("received %d\n", c);
                    if(c>0){
                        fwrite(buf, 1, c, fptr);
                        printf("written to file\n");
                        bytes_to_write = bytes_to_write - c;
                        total_received += c;
                        num_packets++;
                        printf("total received so far: %ld , remaining: %ld, packets so far: %d\n", total_received, bytes_to_write, num_packets);
                    }
                    //send ACK
                    c = sendto(sockfd, ACK, strlen(ACK), 0, (struct sockaddr *)&serveraddr, serverlen);
                }
                fclose(fptr);
                printf("All done!\n");
            }
	    //if command is put
            else if(strncmp(command, "put", 3) == 0){
                printf("sending command %s \n", buf);
                n = 0;
                int c = 0;
		//send command to server
                while( n < strlen(buf)){
                    c = sendto(sockfd, buf+n, strlen(buf)-n, 0, (struct sockaddr *)&serveraddr, serverlen);
                    printf("sent %d\n", c);
                    if(c < 0){
                        error("ERROR in sendto");
                        exit(1);
                    }
                    n += c;
                }
                printf("command sent \n");

                //get file from client
                printf("getting file to send to server\n");
                command = strtok(NULL, " ");
                printf("file to put: %s\n", command);
                if(strncmp(command, "/", 1)==0){
                    strcpy(file_path, command);
                }
                else{
                    strcpy(file_path, "./");
                    strcat(file_path, command);
                }
                printf("attempting to open file >%s<\n", file_path);

                FILE *fp;
                fp = fopen(file_path, "rb");
                // printf("File name is: %s\n", command);
                int errnum;
                int num_read = 1;
                long file_size;
		//check if file opened properly
                if(fp == NULL){
                    errnum = errno;
                    printf("File retrieval failed: %s \n", strerror(errnum));
                    num_read = 0;
                }
                else{
                    printf("File opened successfully \n");
                    //get file size first, so server knows how much to expect
                    struct stat finfo;
                    stat(file_path, &finfo);
                    file_size = finfo.st_size;
                    printf("file size: %ld\n", file_size);
                    //send file size over
                    clearBuffer(buf);
                    snprintf(buf, BUFSIZE, "%zu", file_size); //convert to string
                    printf("File size as string: %s\n", buf);
                    sendto(sockfd, buf, strlen(buf), sendrecvflag, (struct sockaddr*) &serveraddr, serverlen);

                }
                //send contents of file
                long total_sent = 0;
                int num_packets = 0;
                while(total_sent < file_size){
                    clearBuffer(buf);
                    int num_sent = 0;
                    num_read = fread(buf, 1, BUFSIZE, fp);
                    printf("read bytes: %d\n", num_read);
                    while(num_sent < num_read && num_read > 0){
                        num_sent += sendto(sockfd, buf+num_sent, num_read-num_sent, sendrecvflag, (struct sockaddr*) &serveraddr, serverlen);
                        clearBuffer(buf);
                        //wait for ACK
                        n = recvfrom(sockfd, buf, BUFSIZE, 0 , (struct sockaddr*) &serveraddr, &serverlen);
                        //can set up timeout, if no ACK, resend
                        num_packets++;
                        printf("sent bytes: %d\n", num_sent);
                    }
                    total_sent += num_sent;
                    printf("total sent so far: %ld\n", total_sent);
                } 
		//confirm number of bytes sent is the number of bytes read
                printf("total sent: %ld packets %d\n", total_sent, num_packets);
                if(fp != NULL){
                    fclose(fp);
                }
            }
            
            else if(strncmp(command, "ls", 2)==0){
                //send command  ls
                n = 0;
                int c = 0;             
                while(n < strlen(buf)){
                    c = sendto(sockfd, buf+n, strlen(buf)-n, 0, (struct sockaddr*) &serveraddr, serverlen);
                    printf("sent: %d\n", c);
                    if(c < 0){
                        error("ERROR in sendto");
                        exit(1);
                    }
                    n += c;
                }
                printf("Command sent, receiving number of files in directory\n");
                clearBuffer(buf);
		//receive number of files in dir, so we know how much to expect
                c = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                long files_to_get = strtol(buf, NULL, 10);
                printf("Number of files in directory: %ld\n", files_to_get);
                printf("Files in directory: \n");
                while(files_to_get > 0){
	            //receive names of files in dir one by one
                    clearBuffer(buf);
                    c = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &serveraddr, &serverlen);
                    if(c>0){
			 //print files as they are received
                         printf("%s\n", buf);
                         files_to_get--;
                    }
		    //send ACK
                    c = sendto(sockfd, ACK, strlen(ACK), 0, (struct sockaddr *) &serveraddr, serverlen);
                }
                printf("Done!");
            }
            else if(strncmp(buf, "delete", 6) == 0){
               //if command is delete, send command to sevrer and let it handle the rest
	       int c = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
               printf("sent: %d\n", c);
               if(c < 0){
                 error("ERROR in sendto");
                 exit(1);
               }
            }
	    else if(strncmp(buf, "exit", 4)==0){
	       //send exit command to server
               int c = sendto(sockfd, buf, strlen(buf), 0, (struct sockaddr *) &serveraddr, serverlen);
               printf("sent: %d\n", c);
               if(c < 0){
                 error("ERROR in sendto");
                 exit(1);
               }
                //once command sent, exit
                break;
            }
            else{
                printf("ERROR: command not recognized\n");
            }

         }
    }   
