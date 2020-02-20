#ifndef OSS_H_
#define OSS_H_

#define MAX 18


  // system clock
typedef struct system_clock{
  int pids[20];
  unsigned int seconds;
  unsigned int nanoSeconds;

} system_clock;


  //message queue
struct msgbuf{
  long mtype;
  char mtext[256];
};

#endif
[freeman@hoare7 freeman.6]$ clear
[freeman@hoare7 freeman.6]$ cat oss.c
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

  //prototypes
void signalHandler();
void sigAlarmHandler(int sig_num);
int randomInt(int min_num, int max_num);

  // ctrl-c handler variables
int exitFlag = 0;

  // shm pointers and ids
static system_clock* shmClockPtr;
int shmid;  // shared memory id
int msqid;

typedef struct{
  int dirtybit;
  int refByte[20];
  int page;
  int process;

} frameTemplate;

        // MAIN
int main(int argc, char *argv[]){

  int opt;
  int nflag = 18;
  int hflag = 0;
  int vflag = 0;

  while ((opt = getopt(argc, argv, "hn:")) != -1)
    switch(opt){
      case 'h':
        hflag = 1;
        break;
      case 'n':
        nflag = atoi(optarg);
        break;
      case '*':
        break;
      default:
        abort ();
    }

  if(hflag == 1){
    printf("Program simulates operating system paging\n");
    printf(" It forks off cild processes at randomly.\n");
    printf(" that ... .\n");

    return EXIT_SUCCESS;
  }

  if(nflag > MAX){
    nflag = MAX; // hard limit user processes to 18 max
  }

  // file variables
  FILE *ofp = NULL;
  char *ofname = NULL;
  ofname = "logFile.dat";

  // variables for launching child processes
  unsigned int timeToN_Secs;
  unsigned int timeToN_NS;
  unsigned int launchN_Secs;
  unsigned int launchN_NS;

  // msg queue struct
  struct msgbuf message;
  int status;
  // opening output log file to clear it
  ofp = fopen(ofname, "w");
  if(ofp == NULL){
    perror(argv[0]);
    perror("Error oss: Logfile failed to open.");
    return EXIT_FAILURE;
  }
  fclose(ofp);


  // setting up logfile for appending
  ofp = fopen(ofname, "a");
  if(ofp == NULL){
    perror(argv[0]);
    perror("Error oss: Logfile failed to open.");
    return EXIT_FAILURE;
  }


  // master time termination
  alarm(3);


  // catch for ctrl-c signal
  signal(SIGINT, signalHandler);


  // catch alarm signal
  signal(SIGALRM, sigAlarmHandler);

        // KEYS
  // key for shmid
  key_t key = ftok("oss.c", 50);
  if(key == -1){
    perror("Error oss: key shared memory key generation");
    return EXIT_FAILURE;
  }

  key_t skey = ftok("oss.c", 35);
  if(skey ==-1){
    perror("error oss: skey shared memeory key generation");
    return EXIT_FAILURE;
  }

  // key for msg queue
  key_t msgkey = ftok("oss.c", 25);
  if(msgkey == -1){
    perror("Error oss: message queue key generation");
    return EXIT_FAILURE;
  }
        // CREATE SHARED MEMORY
  // might not be large enough
  // shmid system_clock
  shmid = shmget(key, sizeof(system_clock), 0600 | IPC_CREAT);
  if((shmid == -1) && (errno != EEXIST)){
    perror("Error oss: shmget shmid");
    exit(EXIT_FAILURE);
  }


        // ATTACH SHARED MEMORY SEGMENTS
  // attach the segment to our data space system clock
  shmClockPtr = (system_clock *) shmat(shmid, NULL, 0);
  if(shmClockPtr == (void *)-1){
    perror("Error oss: shmat shmClockPtr\n");
    exit(EXIT_FAILURE);
  } else {
    shmClockPtr->seconds = 0;
    shmClockPtr->nanoSeconds = 0;
  }

  // create semaphore
  //unsigned int value = 1;
  sem_t *semlock = sem_open(SEMNAME, FLAGS, PERMS, 1);
  // check semaphore creation
  if(semlock == SEM_FAILED){
    printf("Error oss: semaphore creation failed\n");
    return EXIT_FAILURE;
  }

  // create nessage queue
  if ((msqid = msgget(msgkey, PERMS | IPC_CREAT)) < 0){
    perror("Error oss: msgget\n");
    return EXIT_FAILURE;
  }


  // random time for next process
  srand((unsigned)time(NULL));
  timeToN_Secs = 0;
  timeToN_NS = rand() % maxTimeBetweenNewProcsNS;
  launchN_Secs = timeToN_Secs;
  launchN_NS = launchN_NS + timeToN_NS;
  if( launchN_NS >= BILLION){
    launchN_NS = launchN_NS - BILLION;
    launchN_Secs = launchN_Secs + 1;
  }

  int i, j = 0;
  frameTemplate frameTable[256];
  for(i=0; i < 255; i++){
    frameTable[i].page = -1;
    frameTable[i].process = -1;
    frameTable[i].dirtybit = 0;
  }

  int clfag = 0;
  int pid = 0;
  int pids[20] = {0};
  int n = 0;
  pid_t childpid;
  pid_t waitPID;
  int fcount = 0;
  int pfcount = 0;
  int temp = 0;
  int temp2 = 0;
  int goflag = 0;
  int fflag = 0;
  int pageN;
  int store;
  int pflag = 0;
  int cflag = 0;

  // values for unset message
  message.mtype = 999999;

  printf("STARTING RESOURCE ALLOCATION. . .\n");


         // PRIMARY LOOP
  while (exitFlag == 0){
    // termination flag for breaking outof loop
    if(exitFlag == 1){
      break;
    }
    // increment the clock using shared memory timer
    shmClockPtr->nanoSeconds += 550000;
    if(shmClockPtr->nanoSeconds >= BILLION){
      shmClockPtr->seconds++;
      shmClockPtr->nanoSeconds -= BILLION;
    }

    if(fcount % 100 == 0){
      goflag = 0;
    }

    // print ftable
    if(goflag == 0 && shmClockPtr->seconds != 0 && fcount % 25 == 0){
      fprintf(ofp, "***FRAME TABLE***\n");
      fprintf(ofp, "FRAME NO.   OCCUPIED   PROCESS   PAGE   REFBYTE   DIRTYBYTE\n");
      for(i = 0; i < 256; i++){
        if(i >= 255){break;}
        fprintf(ofp, "   %d   yes \t %d \t %d \t %d \t %d\n", i, frameTable[i].process, frameTable[i].page, 1, frameTable[i].dirtybit);
      }
      goflag = 1;
    }

    /*set flag to identifiy open process space*/
    for(i=0;i<MAX;i++){
      if(pids[i] == 0){
        pflag = 1;
      }
    }

    // printf("gogo\n");
    // launch new processes if necessary
    // printf(" n: %d & fcount: %d & pfcount: %d\n", n, fcount, pfcount);

    // printf(" current-> %d:%d ", shmClockPtr->seconds, shmClockPtr->nanoSeconds);
    // printf(" desired-> %d:%d ", launchN_Secs, launchN_NS);
    if(n < 17 && pflag == 1 && shmClockPtr->seconds >= launchN_Secs
         && shmClockPtr->nanoSeconds >= launchN_NS){
      //printf("inside totalp\n");

      // flag for active child
      cflag = 1;

      char pidNum[5];
      sprintf(pidNum,"%d", pid);

      // fork child process
      childpid = fork();
      //printf("child forked...\n");
      if(childpid < 0){
        printf("ForkError: Error\n");
        //break;
      }
      if(childpid == 0){
        // passing pid to child
        execl("./user", pidNum, NULL);
        perror("Execl Error: failed to exec child from the fork of oss");
        exit(EXIT_FAILURE); //child prog failed to exec
      } else {
        // PARENT PROCESSES
        pids[n] = childpid;

        // attach pid to mtype
        sprintf(message.mtext, "Process: %d", pid);
        pid++; // totalp
        n++; // process Counter
      }

      // random time for next process
      srand((unsigned)time(NULL));
      timeToN_Secs = 0;
      timeToN_NS = rand() % maxTimeBetweenNewProcsNS;
      launchN_Secs = launchN_Secs + timeToN_Secs;
      launchN_NS = launchN_NS + timeToN_NS;

      if( launchN_NS >= BILLION){
        launchN_NS = launchN_NS - BILLION;
        launchN_Secs = launchN_Secs + 1;
      }

    } // end if (processCount < MAX)
    //printf("didnt launch\n");
    //recieve message
    if(cflag == 1){
      //reciving message from user/child
      if(msgrcv(msqid, &message, sizeof(message.mtext), 255255, IPC_NOWAIT) == -1){
      }
    }

    if(message.mtype != 999999){
      //printf("parse message\n");
      // parse msg from child for resources
      int cpid, type, process, address;
      sscanf(message.mtext, "%d/%d/%d/%d", &cpid, &process, &type, &address);
      //printf("proceess and address-> %d:%d\n", process, address);
      /*if  write */
      if(type == 0){
        fprintf(ofp, "Master: %d requesting write of address %d at time %03d:%03d\n", process, address, shmClockPtr->seconds, shmClockPtr->nanoSeconds);
        /*getting frame numbers from address*/
        int pageN = address / 1024;
        int fflag = 0;
        int i = 0;
        int store = 0;
         /*search page table*/
        for(i=0; i<255; i++){
          if(frameTable[i].process=process && frameTable[i].page == pageN){
            fflag = 1;
            break;
          }
        }

        if(fflag != 1){ /*page not in frame*/
          fprintf(ofp, "Master: address %d is not in a frame, pagefault \n", address);
          fcount++;
          pfcount++;
          for(i=0;i<255;i++){
            if(frameTable[i].process==-1 && frameTable[i].page == -1){ /* if available space add page*/
              frameTable[i].process = process;
              frameTable[i].page = pageN;
              frameTable[i].dirtybit = 1;
              store = i;
              break;
            } else if(i==255){
            /*if the frame table is full*/
              frameTable[temp2].process = process;
              frameTable[temp2].page = pageN;
              frameTable[temp2].dirtybit = 1;
              temp2++;
              store = i;
            }
          }
        }
        fprintf(ofp, "Master: Address %d in frame %d, writing data to frame at time %03d:%03d\n",
                address, pageN, shmClockPtr->seconds, shmClockPtr->nanoSeconds);
        fprintf(ofp, "Master: Dirty bit of frame %d set, adding additional time to the clock\n", store);
      } else { /* type == 1*/
        /* read */
        fprintf(ofp, "Master: P%d requesting read of address %d at time %03d:%03d\n",
                process, address, shmClockPtr->seconds, shmClockPtr->nanoSeconds);
        int i = 0;
        int pageN = address/1024;
        int fflag = 0;
        for(i=0;i<255;i++){
          if(frameTable[i].process==process && frameTable[i].page == pageN){
            fflag = 1;
            break;
          }
        }
        if(fflag == 1){ /* page is not in frame */
        fprintf(ofp, "Master: Address %d in frame %d, giving data to P%d at time %03d:%03d\n",
                address, pageN, process, shmClockPtr->seconds, shmClockPtr->nanoSeconds);
        } else {
          fprintf(ofp, "Master: address %d is not in a frame, pagefault.\n", fcount);
          fcount++;
          pfcount++;
          for(i=0;i<255;i++){
            if(frameTable[i].process == -1 && frameTable[i].page == -1){ /*if available space add page*/
              frameTable[i].process = process;
              frameTable[i].page = pageN;
              break;
            } else if(i==255){ /* frame table full*/
              frameTable[temp2].process = process;
              frameTable[temp2].page = pageN;
              temp2++;
            }
          }
        }
        message.mtype = cpid;
        strcpy(message.mtext, "2");
        if(msgsnd(msqid, &message, sizeof(message.mtext) + 1, 0) == -1){
          perror("OSS: msgsnd failed");
        }
      }
      // message not recieved
      message.mtype = 999999;
    }

    //check for terminated processes
    if(n >= 17){
      temp, i = 0;
      for(i=0;i<18;i++){
        waitPID=waitpid(pids[i], &status, WNOHANG);
        if(waitPID>0){
          temp++;
        } else {
          pids[i] = 0;
          n--;
        }
      }
    }

  } // end while(exitFlag == 0)

  int accessAvg = fcount;

        // CLEAN UP
  // wait on child procs
  while((waitPID = wait(&status)) > 0);

  //remove msg queue
  if (msgctl(msqid, IPC_RMID, NULL) == -1) {
    perror("Error oss: msgctl");
  }

  // detach shared memory
  shmdt(shmClockPtr);

  // delete shared memory
  if(shmctl(shmid, IPC_RMID, NULL) == -1){
    perror("Error oss: mshmctl shmid");
  }

  // detach then delete semaphore
  sem_close(semlock);
  sem_unlink(SEMNAME);

  // clean up
  fclose(ofp);

  int memAvg = fcount / 5;
  int pageFaultsPerAccess = pfcount / fcount + 5;
  printf("OUTPUT STATISTICS\n");
  printf("Number of memory access per second %d\n", memAvg);
  printf("Number of page faults per memory access %d\n", pageFaultsPerAccess );
  printf("Average memory access speed %d\n", accessAvg);

  return EXIT_SUCCESS;
}


        // CTRL-C EXIT HANDLER
void signalHandler() {
  exitFlag = 1;
  printf("ctrl-c signal handler executing.\n");

  signal(SIGINT, signalHandler);
}


        // ALARM EXIT HANDLER
void sigAlarmHandler(int sig_num){
  printf("signal alarm exit.\n");
  exitFlag = 1;

  signal(SIGALRM, sigAlarmHandler);
}

// new number each call to random
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