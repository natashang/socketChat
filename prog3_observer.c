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

void printMessage();
void funUsername(int sd);

uint8_t nameLength = 0;
char name[11];

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

  // from server socket
  char buf[1000];
  memset(buf, 0, sizeof(buf)*sizeof(char));
  //receives if you were able to connect or not
  n = recv(sd, &buf, sizeof(char), 0);
  //printf("I am receiving in the first rec: %s \n", buf);

  if (n <= 0){
    //printf("I AM CLOSING THIS SOCKET IN OBSERVER--- first rec\n");
    close(sd);
    exit(1);
  }

  if(*buf == 'N'){
    close(sd);
    exit(1);
  }

  //need to prompt user for username
  if(*buf == 'Y'){
    funUsername(sd);

    memset(buf, 0, sizeof(buf)*sizeof(char));
    n = recv(sd, &buf, sizeof(char), 0);
    if (n <= 0){
      //printf("I AM CLOSING THIS SOCKET IN OBSERVER ONE\n");
      close(sd);
      exit(1);
    }
      //printf("I am receiving in the second rec: %s \n", buf);
    //continues to call username until it receives a valid username
    //needs to reset timeout if receives 'T'
    //does not reset timeout if receives 'I'
    while (*buf == 'T' || *buf == 'N') {

      if (*buf == 'N') {
        close(sd);
        exit(1);
      }

      funUsername(sd);

      memset(buf, 0, sizeof(buf)*sizeof(char));
      n = recv(sd, &buf, sizeof(char), 0);
    }

  }
  //sending/receiving messages now
  while(1){
    uint16_t messageLength;
    n = recv(sd, &messageLength, sizeof(uint16_t), 0); // this works
    if (n <= 0){
      //printf("I AM CLOSING THIS SOCKET IN OBSERVER TWO\n");
      close(sd);
      exit(1);
    }

    memset(buf, 0, sizeof(buf)*sizeof(char));
    n = recv(sd, buf, sizeof(char)*messageLength, 0); // but for some reason this doesn't??

    printf("%s\n", buf);
  }
}

void funUsername(int sd) {
  bool needName = true;

  while(needName){

    struct timeval tv;
    fd_set readfds;

    printf("Enter username: ");




    // timeout or client forcibly closes???
    // TODO: clear out socket space -- on server side


    // Entered in time


            scanf(" %[^\n]s", name);
            nameLength = strlen(name);
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

  }

  //if we are out of while loop it is time to send to the server!!!
  send(sd, &nameLength, sizeof(uint8_t), 0);
  send(sd, name, sizeof(char)*nameLength, 0);

  //if we are about to disconnected change the state to 2
}
