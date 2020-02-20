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