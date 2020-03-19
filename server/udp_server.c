/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUFSIZE 1024
#define cypherKey 'S'
#define sendrecvflag 0
#define ACK "ACK"

extern int errno;
/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/*to clear buffer */
void clearBuffer(char* buffer){
  for(int i = 0; i < BUFSIZE; i++){
    buffer[i] = '\0';
  }
}



int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0){ 
    error("ERROR opening socket");
    exit(2);
  }
  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0){ 
    error("ERROR on binding");
    exit(2);
  }
  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0){
      error("ERROR in recvfrom");
      continue;
    }
    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
       
    /* 
     * sendto: echo the input back to the client 
     */
    printf("buffer contains %s\n", buf);
    if (n < 0) 
      error("ERROR in sendto");
    char* command;
    char file_path[BUFSIZE];
    bzero(file_path, BUFSIZE);
    command = strtok(buf, " ");
    //get command from server
    printf("command contains %s\n", command);
    char* g = "get\0";
    if(strncmp(command, "ls", 2)==0){
      //get filenames in dir for client
      printf("list directory\n");
      printf("getting number of files\n");
      int file_count = 0;
      DIR * dirp;
      struct dirent * entry;
      dirp = opendir(".");
      //get filecount so client knows how much to expect
      while((entry = readdir(dirp)) != NULL){
        if(entry->d_type == DT_REG){
        //regular files only
          file_count++;
        }
      }
      printf("number of files in dir: %d\n", file_count);
      clearBuffer(buf);
      snprintf(buf, BUFSIZE, "%zu", file_count);
      //send num of files to client
      sendto(sockfd, buf, strlen(buf), sendrecvflag, (struct sockaddr*) &clientaddr, clientlen);
      closedir(dirp);
      
      //sent number of files over
      DIR *d;
      struct dirent * dir;
      d = opendir(".");
      if(d){
        while((dir = readdir(d)) != NULL){
          clearBuffer(buf);
          char filenam[255];
          strcpy(filenam, dir->d_name);
          int ss = strlen(filenam);
          printf("size of filename is %d\n", ss);
          strcpy(buf,filenam);
	  //send file names one by one to client
          sendto(sockfd, buf, strlen(buf), sendrecvflag, (struct sockaddr *) &clientaddr, clientlen);
        }
      }
      //close dir
      closedir(d);
      
      
    }
    else if (strncmp(command, "put", 3) == 0){
      printf("put file\n");
      command = strtok(NULL, " "); 
      printf("file to put: %s\n", command);
      if(command == NULL){
        printf("No filename\n");
        continue;
      }
      int c = 0;
      char fname[BUFSIZE];
      strcpy(fname, command);
      printf("writing to path >%s<\n", fname);
      FILE *fptr;
      //create file to write to
      if((fptr = fopen(fname, "wb"))==NULL){
        printf("ERROR opening file\n");
        exit(1);
      }
      printf("Put file opened successfully, getting file size\n");
      clearBuffer(buf);
      //receiving file size first
      c = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr*) &clientaddr, &clientlen);
      printf("file size is: %s\n", buf);
      long bytes_to_write = strtol(buf, NULL, 10);
     // printf("bytes to write: %ld\n", bytes_to_write);
      long total_received = 0;
      int num_packets = 0;
      while(bytes_to_write >0){
	//receive data from client in chunks
        clearBuffer(buf);
        printf("Receiving data...\n");
        c = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr*) &clientaddr, &clientlen);
        printf("Received %d\n", c);
        if(c>0){
          //write data to file as it is received
          fwrite(buf, 1, c, fptr);
          printf("written to file\n");
          bytes_to_write = bytes_to_write - c;
          total_received += c;
          num_packets++;
          printf("total received so far: %ld, remaining: %ld, packets so far: %d\n", total_received, bytes_to_write, num_packets);

        }
	//send ACK
        c = sendto(sockfd, ACK, strlen(ACK), 0, (struct sockaddr*) &clientaddr, clientlen);
      }
      fclose(fptr);
      //close file
      printf("All done!");

    }
    else if(strncmp(command, g, 3)==0){
      printf("getting file\n");
      command = strtok(NULL, " ");
      printf("file to get: %s\n", command);
      if(strncmp(command,"/",1) ==0){
      //absolute path
        strcpy(file_path, command);
      }
      else{
      //use relative path
        strcpy(file_path, "./");
        strcat(file_path, command);
      }
      printf("attempting to open file >%s<\n", file_path);
      //open file
      FILE *fp;
      fp = fopen(file_path, "rb");
     // printf("File name is: %s\n", command);
      int errnum;
      int num_read = 1;
      long file_size;
      if(fp == NULL){
        errnum = errno;
        printf("File retrieval failed: %s \n", strerror(errnum));
        num_read = 0;
      }
      else{
        printf("File opened successfully \n");
        //get file size first, so client knows how much to expect
        struct stat finfo;
        stat(file_path, &finfo);
        file_size = finfo.st_size;
        printf("file size: %ld\n", file_size);
        //send file size over
        clearBuffer(buf);
        snprintf(buf, BUFSIZE, "%zu", file_size); //convert to string
        printf("File size as string: %s\n", buf);
        sendto(sockfd, buf, strlen(buf), sendrecvflag, (struct sockaddr*) &clientaddr, clientlen);
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
          num_sent += sendto(sockfd, buf+num_sent, num_read-num_sent, sendrecvflag, (struct sockaddr*) &clientaddr, clientlen);
          clearBuffer(buf);
          //wait for ACK
          n = recvfrom(sockfd, buf, BUFSIZE, 0 , (struct sockaddr*) &clientaddr, &clientlen);
          //can set up timeout, if no ACK, resend
          num_packets++;
          printf("sent bytes: %d\n", num_sent);
        }
        total_sent += num_sent;
        printf("total sent so far: %ld\n", total_sent);
      }
      //ensure that we sent all the data we read
      printf("total sent: %ld packets %d\n", total_sent, num_packets);
      if(fp != NULL){
        fclose(fp);
      }
    }
    else if(strncmp(command, "delete", 6)==0){
      printf("delete file\n");
      //delete file
      command = strtok(NULL, " ");
      printf("deleting %s\n", command);
      if(remove(command) == 0){
        printf("File deleted successfully!");
      }
      else{
        printf("Unable to delete file");
      }
    }
    else if(strncmp(command, "exit", 4)==0){
      exit(0);
    }
    else{
      printf("unknown command: %s\n", command);
    }
  }
}
    















