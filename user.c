#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <pthread.h>
#include "oss.h"
#include <fcntl.h> /* for O_* constants */
#include <string.h>

#define SEMNAME "/Semfile"
#define PERMS (mode_t) (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define FLAGS (O_CREAT | O_EXCL)
#define MAX 18
#define BILLION 1000000000
#define maxTimeBetweenNewProcsNS 100000000
#define maxTimeBetweenNewProcsSecs 1
#define MAXPROCS 100

int randomInt();
//void signalHandler();

int msqid;
int shmid, shmbid;

int pageTable[32] = {0};

int main(int argc, char *argv[]){

  // file variables
  FILE *ofp = NULL;
  char *ofname = NULL;
  ofname = "logFile.dat";

  // open file for appending
  ofp = fopen(ofname, "a");
  if(ofp == NULL){
    perror("Error user: Logfile failed to open.");
    return EXIT_FAILURE;
  }

  //printf("child/user process launched. . .\n");
  system_clock* shmClockPtr;

  // msg queue struct
  struct msgbuf message;

  // variables
  int index = atoi(argv[0]);

  // signal for quitting
  //signal(SIGINT, signalHandler);

        // KEYS
  // key for shmid
  key_t key = ftok("oss.c", 50);
  if(key == -1){
    perror("Error: shared memory key generation");
    return EXIT_FAILURE;
  }

  // key for msg queue
  key_t msgkey = ftok("oss.c", 25);
  if(msgkey == -1){
    perror("Error: message queue key generation");
    return EXIT_FAILURE;
  }

  sem_t *semlock = sem_open(SEMNAME, 0);

        //CREATE SHARED MEMORY
  // shmid
  shmid = shmget(key, sizeof(system_clock), 0666 | IPC_CREAT);
  if(shmid == -1){
    perror("Error user.c: shmget shmid");
    exit(EXIT_FAILURE);
  }

  // attach the segment to our data space
  shmClockPtr = (system_clock *) shmat(shmid, NULL, 0);
  if(shmClockPtr == (void *)-1){
    perror("Error user.c: shmat shmid");
    exit(EXIT_FAILURE);
  }

  // create message queue
  if ((msqid = msgget(msgkey, 0666 | IPC_CREAT)) < 0){
    perror("Error user.c: msgget");
    return EXIT_FAILURE;
  }

  int i = 0;
  int count = 0;


  // child process memory requests
  while(1){

    int done = 0;
    int try  = 0;
    message.mtype = 255255;
    char text[256];
    int action = 0;
    int address;

    try = randomInt(0,100);
    if(try > 55){
      action = 1; // read
    } else {
      action = 0; // write
    }

    address = randomInt(0, 32000);
    sprintf(text, "%d/%d/%d/%d/%d", getpid(), index, action, address);
    strcpy(message.mtext, text);
    int len = strlen(message.mtext);

    /* send message */
    sem_wait(semlock);
    if (msgsnd(msqid, &message, len + 1, 0) == -1){
      perror("Error user.c: msgsnd failed");
      return -1;
    } else {
      //printf("user msgsnd\n");
    }
    sem_post(semlock);
    count++;

    /*check if process if terminated*/
    if(count > 10){
      break;
    }

  }

  //CLEAN UP
  // detached shared memeory
  shmdt(shmClockPtr);

  // detached semaphore
  sem_close(semlock);

  //printf("child done\n");

  return EXIT_SUCCESS;
}


// randomInt function
int randomInt(int min_num, int max_num){
    int result = 0, low_num = 0, hi_num = 0;
    if (min_num < max_num){
        low_num = min_num;
        hi_num = max_num + 1;
    } else {
        low_num = max_num + 1;
        hi_num = min_num;
    }
    result = (rand() % (hi_num - low_num)) + low_num;
}


  // crl-c signal handler
/*void signalHandler() {
  //exitFlag = 1;
  printf("ctrl-c signal handler executing.\n");
  int i;
  //for(i = 0; i < processCount; i++){
  //  kill(CHILDPIDS[i], SIGKILL);
  //}

  // destroy message queue
  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    perror("Error: msgctl");
  }

  // delete shared memory
  shmctl(shmid, IPC_RMID,NULL);

  signal(SIGINT, signalHandler);

}*/