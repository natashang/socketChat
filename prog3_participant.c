#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <sys/time.h>
#define STDIN 0 // file descriptor for standard input
#define TIMER 4 // time of how long should timer run for

void funUsername(int sd);

void main(int argc, char** argv){
  struct hostent *ptrh; 		/* pointer to a host table entry */
  struct protoent *ptrp; 		/* pointer to a protocol table entry */
  struct sockaddr_in sad; 	/* structure to hold an IP address */
  int sd; 									/* socket descriptor */
  uint16_t port; 						/* protocol port number */
  char *host; 							/* pointer to host name */
  int n; 										/* number of characters read */
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET; 					/* set family to Internet */
  if( argc != 3 ) {
    fprintf(stderr,"Error: Wrong number of arguments\n");
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"./client server_address server_port\n");
    exit(EXIT_FAILURE);
  }

  /* Converts to binary and tests for legal value */
  port = atoi(argv[2]);
  if (port > 0)
  sad.sin_port = htons((u_short)port);
  else {
    fprintf(stderr,"Error: bad port number %s\n",argv[2]);
    exit(EXIT_FAILURE);
  }
  host = argv[1]; 			/* if host argument specified */

  /* Convert host name to equivalent IP address and copy to sad. */
  ptrh = gethostbyname(host);
  if ( ptrh == NULL ) {
    fprintf(stderr,"Error: Invalid host: %s\n", host);
    exit(EXIT_FAILURE);
  }
  memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

  /* Map TCP transport protocol name to protocol number. */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }

  /* Create a socket. */
  sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sd < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }

  /* Connects the socket to the specified server.*/
  if (connect(sd, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
    fprintf(stderr,"connect failed\n");
    exit(EXIT_FAILURE);
  }

  char buf[1000];
  memset(buf, 0, sizeof(buf)*sizeof(char));

  //receives if you were able to connect or not
  n = recv(sd, &buf, sizeof(char), 0);

  if(*buf == 'N'){
    close(sd);
    exit(1);
  }

  //need to prompt user for username
  if(*buf == 'Y'){
    funUsername(sd);

    memset(buf, 0, sizeof(buf)*sizeof(char));
    n = recv(sd, &buf, sizeof(char), 0);
    if(n <= 0){
      printf("I AM CLOSING THIS SOCKET IN PARTICIPANT\n");
      close(sd);
      exit(1);
    }

    //continues to call username until it receives a valid username
    //needs to reset timeout if receives 'T'
    //does not reset timeout if receives 'I'
    while (*buf == 'T' || *buf == 'I') {
      funUsername(sd);

      memset(buf, 0, sizeof(buf)*sizeof(char));
      n = recv(sd, &buf, sizeof(char), 0);
    }

    //sending messages
    while(1) {

      printf("Enter message: ");
      char message[1001] = {'\0'};
      memset(message, 0, sizeof(message)*sizeof(char));
      scanf(" %[^\n]s", message);
      uint16_t messageLength = strlen(message);

      // valid length
      if (messageLength <= 1000 ) {
        send(sd, &messageLength, sizeof(uint16_t), 0);
        send(sd, message, sizeof(char)*messageLength, 0);
      }

      // participant should not send a message longer than 1000 characters
      else {
        printf("Error: Messages are capped at 1000 characters.\n");
      }
    }
  }
}


void funUsername(int sd) {
  bool needName = true;
  char name[11] = {'\0'};
  uint8_t nameLength;

  while(needName){

    //struct timeval tv;
    //fd_set readfds;

    printf("Enter username: ");

    //fflush(STDIN);
    //tv.tv_sec = TIMER;
    //FD_ZERO(&readfds);
    //FD_SET(STDIN, &readfds);
    //int ret = select(STDIN+1, &readfds, NULL, NULL, &tv);

    // select Error
    //if (ret == -1) {
      //perror("select");
    //}

    // timeout or client forcibly closes???
    // TODO:  clear out socket space -- on server side
    //else if (ret == 0) {
    //  printf("TIMEOUT::::::::::\n");
    //  fflush(stdout);
    //  close(sd);
    //  exit(1);
    //}

    // Entered in time
    //else {
      //if (FD_ISSET(STDIN, &readfds)) {
        scanf(" %[^\n]s", name);
        nameLength = strlen(name);
        //printf("The name length i got was: %d \n", nameLength);
        //printf("the name i received was: %s \n", name);

        bool validLength = true;
        bool validChar = true;

        //checking for valid length
        if (nameLength > 10) {
          validLength = false;
        }
        else{

          //checking for valid character
          for (int i = 0; i < nameLength; i++){
            char currChar = name[i];
            if (!(currChar == 95 || (currChar >= 48 && currChar <= 57) || (currChar >= 65 && currChar <= 90) || (currChar >= 97 && currChar <= 122))){
              validChar = false;
            }
          }
        }

        if(validLength && validChar){
          needName = false;
        }
      //}
    //}
  }

  //if we are out of while loop it is time to send to the server!!!
  send(sd, &nameLength, sizeof(uint8_t), 0);
  //printf("The name length I am sending is: %d \n", nameLength);
  send(sd, name, sizeof(char)*nameLength, 0);

}
