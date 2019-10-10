#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>

/* Macros */
#define QLEN 6      /* size of request queue */
#define MAXSIZE 255 /* maximum number of participants/sockets */
#define TIMER 4     /* time of how long should timer run for */

/* client struct fields:
- sdparts: if participant, tells you what socket you are
if observer, tells you if you are connected to a participant
- sdobs: if participant, tells you if you are connected to a participant
if observer, tells you what socket you are
- name: tells you your name
- state: just connected (0)
active (1)
not connected (-1)
*/
typedef struct client{
  int sdparts;
  int sdobs;
  char name[11];
  int state;
  double time;
  struct timeval start;
  struct timeval end;
} client;

/* Prototypes --------------------------------------------------------*/

// sets default values for participants and observers array
void initializeSDs();

/* connectingPart
 *    A participant is attempting to connect
 */
int connectingPart(int sdpart, struct sockaddr_in cad, int alen);

/* connectingObs:
 *   An observer is attempting to connect
 */
int connectingObs(int sdobs, struct sockaddr_in cad, int alen);

/* usernamePart
 *    Prompts participant for a username
 *    Inputted username must be:
 *       consists of lower/uppercase letters, numbers, and underscores
 *       between 1-10 characters long
 *    If already taken username, the user is reprompted to enter again
 *    If no username inputted in time, the server disconnects the participant
*/
void usernamePart(int sdpart, int j, fd_set readfds);

/* usernameObs
*   Inputted username must match that of an active participant
*   Server disconnects the observer if:
*      name is not inputted in time
*      name does not match that of an active participant
*/
void usernameObs(int sdobs, int j, fd_set readfds);

/* doMessage
 *    Handles private, public, new observer, participant joining/leaving
 *    Handles active participant disconnecting
 *    Prepends messages using concat method
 *    Limits messages to 1000 characters
 */
void doMessage(int j, fd_set readfds);

/* disconnectPart
 *    If a participant disconnects, all observers are informed
 *    Affiliated observer is also disconnected
 */
void disconnectPart(int j, fd_set readfds);

/* disconnectObs
 *    If an observer disconnects, no message is printed
 *    The observer can reconnect with the same username
 */
void disconnectObs(int j, fd_set readfds);

/* resetObsSD
 *    Invoked when an observer disconnects
 *    If an active participant disconnects, the affiliated observer disconnects as well
 *    Resets default values for observer array
 */
void resetObsSD(int j);

/* resetPartSD
 *    Invoked when a participant disconnects
 *    Resets default values for participant array
 */
void resetPartSD(int j);

/* concat
 *    Helper function
 *    Concatenates three strings for message printing
 */
char* concat(const char *str1, const char *str2, const char *str3);

/* -------------------------------------------------------------------*/


/* Global variables */
client participants[MAXSIZE];
int pSize = 0;
client observers[MAXSIZE];
int oSize = 0;

int main(int argc, char** argv) {
  srand(time(0));
  struct protoent *ptrp;  	/* pointer to a protocol table entry */
  struct sockaddr_in sad; 	/* structure to hold server's address */
  struct sockaddr_in cad; 	/* structure to hold client's address */
  int sdpart, sdobs;         /* socket descriptors */
  int alen;               	/* length of address */
  int optval = 1;         	/* boolean value when we set socket option */
  uint16_t participantPort;
  uint16_t observerPort;    /* protocol port number */

  if( argc != 3 ) {
    fprintf(stderr,"Error: Wrong number of arguments\n");
    fprintf(stderr,"usage:\n");
    fprintf(stderr,"./server server_port\n");
    exit(EXIT_FAILURE);
  }

  participantPort = atoi(argv[1]);
  observerPort = atoi(argv[2]);

  //creates and does the port stuff for the particpant port
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;
  sad.sin_addr.s_addr = INADDR_ANY;

  /* Tests for illegal value, sets port number */
  if (participantPort > 0) {
    sad.sin_port = htons(participantPort);
  }
  else {
    fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
    exit(EXIT_FAILURE);
  }
  /* Map TCP transport protocol name to protocol number */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }
  /* Creates a socket and returns a socket descriptor named sd. */
  sdpart = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sdpart < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }
  /* Allow reuse of port - avoid "Bind failed" issues */
  if( setsockopt(sdpart, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
    fprintf(stderr, "Error Setting socket option failed\n");
    exit(EXIT_FAILURE);
  }
  /* Bind a local address to the socket. */
  if (bind(sdpart, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
    fprintf(stderr,"Error: Bind failed\n");
    exit(EXIT_FAILURE);
  }
  /* Specify size of request queue. */
  if (listen(sdpart, QLEN) < 0) {
    fprintf(stderr,"Error: Listen failed\n");
    exit(EXIT_FAILURE);
  }

  //creates and does the port stuff for the observer port
  memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
  sad.sin_family = AF_INET;
  sad.sin_addr.s_addr = INADDR_ANY;

  /* Tests for illegal value, sets port number */
  if (observerPort > 0) {
    sad.sin_port = htons(observerPort);
  }
  else {
    fprintf(stderr,"Error: Bad port number %s\n",argv[2]);
    exit(EXIT_FAILURE);
  }
  /* Map TCP transport protocol name to protocol number */
  if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
    exit(EXIT_FAILURE);
  }
  /* Creates a socket and returns a socket descriptor named sd. */
  sdobs = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
  if (sdobs < 0) {
    fprintf(stderr, "Error: Socket creation failed\n");
    exit(EXIT_FAILURE);
  }
  /* Allow reuse of port - avoid "Bind failed" issues */
  if( setsockopt(sdobs, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
    fprintf(stderr, "Error Setting socket option failed\n");
    exit(EXIT_FAILURE);
  }
  /* Bind a local address to the socket. */
  if (bind(sdobs, (struct sockaddr *) &sad, sizeof(sad)) < 0) {
    fprintf(stderr,"Error: Bind failed\n");
    exit(EXIT_FAILURE);
  }
  /* Specify size of request queue. */
  if (listen(sdobs, QLEN) < 0) {
    fprintf(stderr,"Error: Listen failed\n");
    exit(EXIT_FAILURE);
  }

  fd_set readfds;
  int status;
  int max = sdobs;
  int n;

  // Initialize participants and observers
  initializeSDs();

  while(1){

    status = 0;
    FD_ZERO(&readfds);
    FD_SET(sdpart, &readfds);
    FD_SET(sdobs, &readfds);
    max = sdobs;

    for (int i=0; i<MAXSIZE; i++){
      FD_SET(participants[i].sdparts, &readfds);
      if(participants[i].sdparts > max){
        max = participants[i].sdparts;
      }
      FD_SET(observers[i].sdobs, &readfds);
      if(observers[i].sdobs > max){
        max = observers[i].sdobs;
      }
    }

    printf("max+1: %d\n", max+1);
    status = select (max+1, &readfds, NULL, NULL, NULL);
    printf("status:%d\n", status);

    if (status == -1) {
      //printf("I AM -1, max = %d\n", max);
      perror("select");
      strerror(errno);
      exit(1);
    }

    // someone closed
    else if (status == 0) {
      printf("someone closed\n");
      exit(1);
    }

    // game logic
    else {
      int j;

      // A participant is connecting
      if (FD_ISSET(sdpart, &readfds)) {
        j =  connectingPart(sdpart, cad, alen);
      }

      // An observer is connecting
      else if (FD_ISSET(sdobs, &readfds)) {
        j = connectingObs(sdobs, cad, alen);
      }

      // Messages
      else{
        for(int i = 0; i < MAXSIZE; i++){
          if(FD_ISSET(participants[i].sdparts, &readfds)){

            // Connected participants need a username
            if (participants[i].state == 0) {
              usernamePart(participants[i].sdparts, j, readfds);
            }

            // Sending a message
            else {
              doMessage(i, readfds);
            }
          }

          // Observer is trying to send something to server
          // The observer never sends anything to server
          if(FD_ISSET(observers[i].sdobs, &readfds)){

            // Getting a username to associate with participant
            if(observers[i].state == 0){
              usernameObs(observers[i].sdobs, j, readfds);
            }
            else{
              //someone quit
              printf("an observer quit\n");
              for (int k = 0; k < MAXSIZE; k++) {
                if (0 == strcmp(participants[k].name, observers[i].name) ) {
                  close(observers[i].sdobs);
                  participants[k].sdobs = 0;
                  printf("CLOSING ACTIVE PARTICIPANT'S OBSERVER SOCKET\n");
                  FD_CLR(observers[i].sdobs, &readfds);
                  oSize--;
                  resetObsSD(i);
                  k = MAXSIZE;

                }
              }
            }
          }
        }
      }
    } //else
  }   //while game continues
} //main


void doMessage(int j, fd_set readfds) {
  int n;
  uint16_t messageLength;

  // Active participant disconnected
  n = recv(participants[j].sdparts, &messageLength, sizeof(uint16_t), 0);
  if (n <= 0) {
    disconnectPart(j, readfds);
    //disconnectObs(j, readfds);

    // close(participants[j].sdparts);
    // printf("CLOSING ACTIVE PARTICIPANT SOCKET\n");
    //
    // char* username = malloc(sizeof(char) * strlen(participants[j].name));
    // strcpy(username, participants[j].name);
    // char* msgToSend = concat("User ", username, " has left");
    // uint16_t msgLength = strlen(msgToSend);
    //
    // // sends in network standard byte order
    // msgLength = htons(ntohs(msgLength));
    //
    // char weWillSendThisMsg[msgLength];
    // for(int k = 0; k < msgLength; k++){
    //   weWillSendThisMsg[k] = msgToSend[k];
    // }
    //
    // for (int i = 0; i < MAXSIZE; i++) {
    //   // send to all observers except for the one affiliated with disconnecting participant
    //   if(observers[i].sdobs != 0 && observers[i].sdobs != participants[j].sdobs){
    //     send(observers[i].sdobs, &msgLength, sizeof(uint16_t), 0);
    //     send(observers[i].sdobs, weWillSendThisMsg, sizeof(char)*msgLength, 0);
    //   }
    // }
    //
    // for (int i = 0; i < MAXSIZE; i++) {
    //   if (0 == strcmp(participants[j].name, observers[i].name) ) {
    //     close(observers[i].sdobs);
    //     printf("CLOSING ACTIVE PARTICIPANT'S OBSERVER SOCKET\n");
    //     FD_CLR(observers[i].sdobs, &readfds);
    //     oSize--;
    //     resetObsSD(i);
    //   }
    // }
    //
    // FD_CLR(participants[j].sdparts, &readfds);
    // pSize--;
    // resetPartSD(j);

  }

  else {
    char message[1001] = {'\0'};
    memset(message, 0, sizeof(message)*sizeof(char));
    n = recv(participants[j].sdparts, message, sizeof(char)*messageLength, 0);

    if (messageLength >= 1000) {
      printf("we will disconnect participants[j]");
      disconnectPart(j, readfds);
      //disconnectObs(j, readfds);
    }
    else {

      // private
      if (message[0] == '@' && message[1] != ' ') {
        int p = 1;
        int recipNameLength = 0;
        // case 1: @username

        // gets recipient name length
        while (message[p] != ' '){
          recipNameLength++;
          p++;
        }

        // stores recipient's name
        char* recipName = calloc(recipNameLength, sizeof(char));
        int q = 0;
        while(message[q+1] != ' ' && q+1 != p) {
          recipName[q] = message[q+1];
          q++;
        }

        // finds the length of the message
        p++;
        int startMessage = p;
        uint16_t messageLengthCurr =0;
        while (message[p] != '\0'){
          messageLengthCurr++;
          p++;
        }

        // stores the message
        char* messageCurr = calloc(messageLengthCurr, sizeof(char));
        p = startMessage;
        q = 0;
        while(message[p] != '\0'){
          messageCurr[q] = message[p];
          q++;
          p++;
        }

        // checks if recipient is active
        bool validRecip = false;
        int recipObs = 0;
        for(int i = 0; i < MAXSIZE; i++){
          if(strcmp(observers[i].name, recipName) == 0 ){
            recipObs = i;
            validRecip = true;
            i = MAXSIZE;
          }
        }

        if (validRecip) {
          int senderNameLen = strlen(participants[j].name);
          int numSpaces = 11- senderNameLen;

          char spacesStr[14] = {'\0'};
          for (int i = 0; i < numSpaces; i++) {
            spacesStr[i] = ' ';
          }

          char* msgToSend = concat("-", spacesStr, participants[j].name);
          msgToSend = concat(msgToSend, ": ", messageCurr);

          uint16_t msgLength = strlen(msgToSend);
          char weWillSendThisMsg[msgLength];
          for(int k = 0; k < msgLength; k++){
            weWillSendThisMsg[k] = msgToSend[k];
          }

          msgLength = htons(ntohs(msgLength));

          // send to observer affiliated with recipient
          if(observers[recipObs].sdobs != 0){
            send(observers[recipObs].sdobs, &msgLength, sizeof(uint16_t), 0);
            send(observers[recipObs].sdobs, weWillSendThisMsg, sizeof(char)*msgLength, 0);
          }

          // send to observer affiliated with sender
          if(participants[j].sdobs != 0){
            send(participants[j].sdobs, &msgLength, sizeof(uint16_t), 0);
            send(participants[j].sdobs, weWillSendThisMsg, sizeof(char)*msgLength, 0);
          }
        }

        else {
          char* msgToSend = concat("Warning: user ", recipName, " doesn't exist...");

          uint16_t msgLength = strlen(msgToSend);
          char weWillSendThisMsg[msgLength];
          for(int k = 0; k < msgLength; k++){
            weWillSendThisMsg[k] = msgToSend[k];
          }

          msgLength = htons(ntohs(msgLength));

          // send to observer affiliated with sender
          send(participants[j].sdobs, &msgLength, sizeof(uint16_t), 0);
          send(participants[j].sdobs, weWillSendThisMsg, sizeof(char)*msgLength, 0);
        }

        // case 2: @ a message // TODO: THIS:::::::::::::::::::::::::
      }

      //public
      else {
        uint8_t nameLength = strlen(participants[j].name);
        int numSpaces = 11-nameLength;

        char spacesStr[14] = {'\0'};
        for (int i = 0; i < numSpaces; i++) {
          spacesStr[i] = ' ';
        }

        char* msgToSend = concat(">", spacesStr, participants[j].name);
        msgToSend = concat(msgToSend, ": ", message);

        uint16_t msgLength = strlen(msgToSend);
        char currMessage[msgLength];
        for(int k = 0; k < msgLength; k++){
          currMessage[k] = msgToSend[k];
        }

        msgLength = htons(ntohs(msgLength));

        for(int i = 0; i < MAXSIZE; i++){
          if((observers[i].sdobs != 0)){
            send(observers[i].sdobs, &msgLength, sizeof(uint16_t), 0);
            send(observers[i].sdobs, currMessage, sizeof(char)*msgLength, 0);
          }
        }
      }
    }
  }



}

int connectingPart(int sdpart, struct sockaddr_in cad, int alen){
  bool notFound = true;
  int j = 0;
  while (j < MAXSIZE && notFound){

    if(participants[j].sdparts == 0){
      notFound = false;
    }
    else{
      j++;
    }
  }
  alen = sizeof(cad);

  //array is full
  if(j == MAXSIZE){
    int sdFULL;
    if ((sdFULL = accept(sdpart, (struct sockaddr *)&cad, &alen)) < 0) {
      fprintf(stderr, "Error: Accept failed\n");
      exit(EXIT_FAILURE);
    }
    char buf2[]={'N'};
    send(sdFULL, &buf2, sizeof(char), 0);
    close(sdFULL);
  }

  else{
    if ((participants[j].sdparts=accept(sdpart, (struct sockaddr *)&cad, &alen)) < 0) {
      fprintf(stderr, "Error: Accept failed\n");
      exit(EXIT_FAILURE);
    }

    // case 1: connection
    if (participants[j].state  == -1) {
      if (pSize < MAXSIZE){
        char buf[]={'Y'};
        send(participants[j].sdparts, &buf, sizeof(char), 0);
        //struct timeval start;
        //struct timeval end;
        gettimeofday(&participants[j].start, NULL);
        participants[j].state  = 0;
        pSize++;
      }
    }
  }
  return j;
}

int connectingObs(int sdobs, struct sockaddr_in cad, int alen){

  bool notFound = true;
  int j = 0;
  while (j < MAXSIZE && notFound){
    if(observers[j].sdobs == 0){
      notFound = false;
    }
    else{
      j++;
    }
  }
  alen = sizeof(cad);

  //array is full
  if(j == MAXSIZE){
    int sdFULL;
    if ((sdFULL = accept(sdobs, (struct sockaddr *)&cad, &alen)) < 0) {
      fprintf(stderr, "Error: Accept failed\n");
      exit(EXIT_FAILURE);
      char buf2[]={'N'};
      send(sdFULL, &buf2, sizeof(char), 0);
      close(sdFULL);
    }
  }

  else{
    if ((observers[j].sdobs=accept(sdobs, (struct sockaddr *)&cad, &alen)) < 0) {
      fprintf(stderr, "Error: Accept failed\n");
      exit(EXIT_FAILURE);
    }

    // case 1: connection
    if (observers[j].state  == -1) {
      if (oSize < MAXSIZE){
        char buf[]={'Y'};
        send(observers[j].sdobs, &buf, sizeof(char), 0);
        //gettimeofday(&observers[j].start, NULL);
        observers[j].state  = 0;
        oSize++;
      }
    }
  }
  return j;
}

void usernamePart(int sdpart, int j, fd_set readfds){
  int n;
  uint8_t nameLength;
  bool needName = true;
  while(needName){

    // asking/checking username of participants
    if(participants[j].state == 0){
      n = recv(participants[j].sdparts, &nameLength, sizeof(uint8_t), 0);

      // close everything
      if (n == 0) {
        close(participants[j].sdparts);
        printf("I AM CLOSING THE SOCKET IN PARTICPANT USER \n");
        FD_CLR(participants[j].sdparts, &readfds);
        pSize--;
        resetPartSD(j);

      }
      else{

        char name[11] = {'\0'};
        for (int i = 0; i < 11; i++) {
          name[i] = '\0';
        }
        n = recv(participants[j].sdparts, name, sizeof(char)*nameLength, 0);
        gettimeofday(&participants[j].end, NULL);
        double timeTaken = (double)(participants[j].end.tv_usec - participants[j].start.tv_usec)/1000000 + (participants[j].end.tv_sec - participants[j].start.tv_sec);
        if(timeTaken > participants[j].time){

          needName = false;
          close(participants[j].sdparts);
          FD_CLR(participants[j].sdparts, &readfds);
          pSize--;
          resetPartSD(j);

          //ran out of time
        }
        else{
          participants[j].time = participants[j].time - timeTaken;
          //didn't run out of time

          bool validLength = true;
          if (nameLength > 10){
            validLength = false;
          }

          bool validChar = true;
          for (int i = 0; i < nameLength; i++){
            char currChar = name[i];
            if (!(currChar == 95 || (currChar >= 48 && currChar <= 57) || (currChar >= 65 && currChar <= 90) || (currChar >= 97 && currChar <= 122))){
              validChar = false;
            }
          }

          name[nameLength] = '\0';
          bool alreadyGuessed = false;
          for(int a = 0; a < MAXSIZE; a++){
            if(strcmp(participants[a].name, name) == 0){
              //no observer yet, send "I"
              a = MAXSIZE;
              alreadyGuessed = true;
            }
          }

          if(validLength && validChar && !alreadyGuessed){

            strcpy(participants[j].name, name);
            participants[j].state = 1;

            //send 'Y' to participant
            char buf[] = {'Y'};
            needName = false;
            send(participants[j].sdparts, &buf, sizeof(char), 0);

            // send the name to everybody that "a new 'username' joined"
            uint16_t messageSize = 18 + nameLength;
            char* str1 = "User ";
            // str2 will be name
            char* str3 = " has joined";
            char* message = concat(str1, name, str3);
            char messageCurr[messageSize+1];

            for(int k = 0; k < messageSize; k++){
              messageCurr[k] = message[k];
            }

            messageSize = htons(ntohs(messageSize));


            for(int i = 0; i < MAXSIZE; i++){
              if((observers[i].sdobs != 0) && (observers[i].sdobs != participants[j].sdobs)){
                send(observers[i].sdobs, &messageSize, sizeof(uint16_t), 0);
                send(observers[i].sdobs, messageCurr, sizeof(char)*messageSize, 0);
              }
            }
          }

          else{
            if(validLength == false || validChar == false){

              //send 'I'
              //do not reset timer
              char buf[] = {'I'};
              send(participants[j].sdparts, &buf, sizeof(char), 0);
            }

            if(alreadyGuessed){

              //send 'T'
              //reset timer
              char buf[] = {'T'};
              send(participants[j].sdparts, &buf, sizeof(char), 0);
            }
          } //end else
        } // if participant state = 0
      }
    }
  } // else select > 0

}

void usernameObs(int sdobs, int j, fd_set readfds){
  int n;
  uint8_t nameLength;
  bool needName = true;
  while (needName){
    if(observers[j].state == 0){

      n = recv(observers[j].sdobs, &nameLength, sizeof(uint8_t), 0);


      if (n <= 0) {

        // this was closing active participant
        //close(participants[j].sdparts);
        printf("CLOSING ACTIVE OBSERVER SOCKET\n");
        disconnectObs(j, readfds);

        // char* username = malloc(sizeof(char) * strlen(participants[j].name));
        // strcpy(username, participants[j].name);
        // char* msgToSend = concat("User ", username, " has left");
        // uint16_t msgLength = strlen(msgToSend);
        //
        // char weWillSendThisMsg[msgLength];
        // for(int k = 0; k < msgLength; k++){
        //   weWillSendThisMsg[k] = msgToSend[k];
        // }
        //
        // for (int i = 0; i < MAXSIZE; i++) {
        //   // send to all observers except for the one affiliated with disconnecting participant
        //   if(observers[i].sdobs != 0 && observers[i].sdobs != participants[j].sdobs){
        //     send(observers[i].sdobs, &msgLength, sizeof(uint16_t), 0);
        //     send(observers[i].sdobs, weWillSendThisMsg, sizeof(char)*msgLength, 0);
        //   }
        // }
        //
        // for (int i = 0; i < MAXSIZE; i++) {
        //   if (0 == strcmp(participants[j].name, observers[i].name) ) {
        //     close(observers[i].sdobs);
        //     printf("CLOSING ACTIVE PARTICIPANT'S OBSERVER SOCKET\n");
        //     FD_CLR(observers[i].sdobs, &readfds);
        //     oSize--;
        //     observers[i].sdparts = 0;
        //     observers[i].sdobs = 0;
        //     memset(observers[i].name, 0, sizeof(observers[i].name));
        //     observers[i].state = -1;
        //     observers[i].time = TIMER;
        //
        //   }
        // }
        //
        // FD_CLR(participants[j].sdparts, &readfds);
        // pSize--;
        // /*
        // participants[j].sdparts = 0;
        // participants[j].sdobs = 0;
        // memset(participants[j].name, 0, sizeof(participants[j].name));
        // participants[j].state = -1;
        // participants[j].time = TIMER;
        // */
        // resetPartSD(j);
        //

      }


      printf("The length of the name we are receivng is : %d \n", nameLength);
      //close everything
      /*if (n == 0) {
        printf("I AM CLOSING THE SOCKET IN ObserverUSER \n");
        FD_CLR(observers[j].sdobs, &readfds);
        oSize--;
        observers[j].sdparts = 0;
        observers[j].sdobs = 0;
        memset(observers[j].name, 0, sizeof(observers[j].name));
        observers[j].state = -1;
        observers[j].time = 4;

        close(observers[j].sdobs);
      }*/
      //else{

        char name[11] = {'\0'};
        for (int i = 0; i < 11; i++) {
          name[i] = '\0';
        }

        //gettimeofday(&observers[j].end, NULL);
        ///double timeTaken = (double)(observers[j].end.tv_usec - observers[j].start.tv_usec)/1000000 + (observers[j].end.tv_sec - observers[j].start.tv_sec);
        //bool noMatch = true;
        //printf("timeTaken: %f, observers[j].time: %f\n", timeTaken, observers[j].time);
        /*if(timeTaken > observers[j].time){
          int status_test = 0;
          printf("I AM CLOSING THE SOCKET IN ObserverUSER IN PLACE #2\n");
          needName = false;
          noMatch = false;
          status_test = close(observers[j].sdparts);
          fprintf(stderr, "status 1 is %d\n", status_test);
          fprintf(stderr, "status 2 is %d\n", observers[j].sdobs);
          FD_CLR(observers[j].sdobs, &readfds);
          oSize--;
          // fprintf(stderr, "status 2 is %d\n", status_test);
          observers[j].sdparts = 0;
          observers[j].sdobs = 0;
          memset(observers[j].name, 0, sizeof(observers[j].name));
          // fprintf(stderr, "status 3 is %d\n", status_test);
          observers[j].state = -1;
          observers[j].time = 4;
          observers[j].start;
          observers[j].end;

          //ran out of time
        }*/
        //else{
          //observers[j].time = observers[j].time - timeTaken;

          n = recv(observers[j].sdobs, &name, sizeof(char)*nameLength, 0);
          //observers[j].time = observers[j].time - timeTaken;

          bool noMatch = true;
          for(int a = 0; a < MAXSIZE; a++){
            printf("the participants name is: %s \n", participants[a].name);
            printf("the given name is: %s \n", name);
            if(strcmp(participants[a].name, name) == 0){

              //no observer yet, send "I"
              if(participants[a].sdobs == 0){
                printf("I found a participant with the name I'm looking for!! \n");
                needName = false;
                char buf={'Y'};
                send(observers[j].sdobs, &buf, sizeof(char), 0);
                participants[a].sdobs = observers[j].sdobs;
                observers[j].sdparts  = participants[a].sdparts;
                observers[j].state    = 1;
                strcpy(observers[j].name, name);

                //send the name to everybody that "a new observer joined"
                uint16_t messageSize = 25;
                char message[25] = ("A new observer has joined");

                messageSize = htons(ntohs(messageSize));

                for(int i = 0; i < MAXSIZE; i++){
                  if((observers[i].sdobs != 0) && (observers[i].sdobs != participants[a].sdobs)){
                    send(observers[i].sdobs, &messageSize, sizeof(uint16_t), 0);
                    send(observers[i].sdobs, message, sizeof(char)*messageSize, 0);
                  }
                }
              }

              //they already have an observer send 'T'
              else{
                char buf= {'T'};
                send(observers[j].sdobs, &buf, sizeof(char), 0);
              }
              a = MAXSIZE;
              noMatch = false;
            }
          }
          if(noMatch){
            printf("I got into the case where I didn't find the name \n");
            char buf2[]={'N'};
            needName = false;
            send(observers[j].sdobs, &buf2, sizeof(char), 0);
            oSize--;
            FD_CLR(observers[j].sdobs, &readfds);
            close(observers[j].sdobs);

            resetObsSD(j);

          }
        }
      //}
    //}
  }
}

void disconnectPart(int j, fd_set readfds) {
  close(participants[j].sdparts);
  printf("CLOSING ACTIVE PARTICIPANT SOCKET\n");

  char* username = malloc(sizeof(char) * strlen(participants[j].name));
  strcpy(username, participants[j].name);
  char* msgToSend = concat("User ", username, " has left");
  uint16_t msgLength = strlen(msgToSend);

  // sends in network standard byte order
  msgLength = htons(ntohs(msgLength));

  char weWillSendThisMsg[msgLength];
  for(int k = 0; k < msgLength; k++){
    weWillSendThisMsg[k] = msgToSend[k];
  }

  for (int i = 0; i < MAXSIZE; i++) {
    // send to all observers except for the one affiliated with disconnecting participant
    if(observers[i].sdobs != 0 ){
      send(observers[i].sdobs, &msgLength, sizeof(uint16_t), 0);
      send(observers[i].sdobs, weWillSendThisMsg, sizeof(char)*msgLength, 0);
    }
  }
  for (int i = 0; i < MAXSIZE; i++) {
    if (0 == strcmp(participants[j].name, observers[i].name) ) {
      close(observers[i].sdobs);
      printf("CLOSING ACTIVE PARTICIPANT'S OBSERVER SOCKET\n");
      FD_CLR(observers[i].sdobs, &readfds);
      oSize--;
      resetObsSD(i);
    }
    i = MAXSIZE;
  }


  for (int i = 0; i < MAXSIZE; i++) {
    if (0 == strcmp(participants[j].name, observers[i].name) ) {
      close(observers[i].sdobs);
      printf("CLOSING ACTIVE PARTICIPANT'S OBSERVER SOCKET\n");
      FD_CLR(observers[i].sdobs, &readfds);
      oSize--;
      resetObsSD(i);
    }
  }

  FD_CLR(participants[j].sdparts, &readfds);
  pSize--;
  resetPartSD(j);
}

void disconnectObs(int j, fd_set readfds) {
  for (int i = 0; i < MAXSIZE; i++) {
    if (0 == strcmp(participants[j].name, observers[i].name) ) {
      close(observers[i].sdobs);
      printf("CLOSING ACTIVE PARTICIPANT'S OBSERVER SOCKET\n");
      FD_CLR(observers[i].sdobs, &readfds);
      oSize--;
      resetObsSD(i);
    }
    i = MAXSIZE;
  }
}

void resetObsSD(int j) {
  observers[j].sdparts = 0;
  observers[j].sdobs = 0;
  memset(observers[j].name, 0, sizeof(observers[j].name));
  observers[j].state = -1;
  observers[j].time = TIMER;
  observers[j].start;
  observers[j].end;
}

void resetPartSD(int j) {
  participants[j].sdparts = 0;
  participants[j].sdobs = 0;
  memset(participants[j].name, 0, sizeof(participants[j].name));
  participants[j].state = -1;
  participants[j].time = TIMER;
  participants[j].start;
  participants[j].end;
}

void initializeSDs() {

  for (int i = 0; i < 255; i++) {
    participants[i].sdparts = 0;
    participants[i].sdobs = 0;
    memset(participants[i].name, 0, sizeof(participants[i].name));
    participants[i].state = -1;
    participants[i].time = TIMER;
    participants[i].start;
    participants[i].end;
  }

  for (int i = 0; i < 255; i++) {
    observers[i].sdparts = 0;
    observers[i].sdobs = 0;
    memset(observers[i].name, 0, sizeof(observers[i].name));
    observers[i].state = -1;
    observers[i].time = TIMER;
    observers[i].start;
    observers[i].end;

  }
}

char* concat(const char *str1, const char *str2, const char *str3) {

  char* result = malloc (sizeof(char) * (strlen(str1) + strlen(str2) + strlen(str3) + 1)); //+1 for null terminator
  if (result == NULL) {
    printf("out of memory\n");
    exit(1);
  }

  strcpy(result, str1);
  strcat(result, str2);
  strcat(result, str3);

  return result;
}
