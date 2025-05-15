// oss.c
// Author: Curtis Been
// Description: Master process for memory management simulation using LRU

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_PROCESSES 18
#define MAX_TOTAL_PROCESSES 100
#define PAGE_SIZE 1024
#define NUM_PAGES 32
#define FRAME_TABLE_SIZE 256
#define MEMORY_SIZE (PAGE_SIZE * FRAME_TABLE_SIZE)
#define MSGKEY 1234
#define SHMKEY 5678
#define BILLION 1000000000

typedef struct {
    int seconds;
    int nanoseconds;
} SimClock;

typedef struct {
    int occupied;
    int dirtyBit;
    int pid;
    int pageNumber;
    SimClock lastRef;
} Frame;

typedef struct {
    int frameNumber[NUM_PAGES];
} PageTable;

typedef struct {
    SimClock clock;
    Frame frameTable[FRAME_TABLE_SIZE];
    PageTable pageTables[MAX_PROCESSES];
} SharedData;

typedef struct {
    long mtype;
    int pid;
    int address;
    int isWrite;
} MessageToOSS;

typedef struct {
    long mtype;
    int granted;
} MessageFromOSS;

int shm_id, msg_id;
SharedData *shared;
FILE *logfile;

void cleanup(int sig) {
    if (shm_id != -1) shmctl(shm_id, IPC_RMID, NULL);
    if (msg_id != -1) msgctl(msg_id, IPC_RMID, NULL);
    if (logfile) fclose(logfile);
    kill(0, SIGTERM);
    exit(0);
}

void advanceClock(SimClock *clock, int sec, int ns) {
    clock->nanoseconds += ns;
    clock->seconds += sec + clock->nanoseconds / BILLION;
    clock->nanoseconds %= BILLION;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, cleanup);
    srand(time(NULL));

    logfile = fopen("oss.log", "w");
    if (!logfile) {
        perror("fopen");
        exit(1);
    }

    shm_id = shmget(SHMKEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    shared = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    memset(shared, 0, sizeof(SharedData));

    msg_id = msgget(MSGKEY, IPC_CREAT | 0666);
    if (msg_id < 0) {
        perror("msgget");
        exit(1);
    }

    // Basic launch of one process and communication test
    pid_t pid = fork();
    if (pid == 0) {
        execl("./user_proc", "user_proc", NULL);
        perror("execl");
        exit(1);
    }

    while (1) {
        MessageToOSS msg;
        if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1) continue;

        fprintf(logfile, "oss: Received %s request for address %d from PID %d\n",
                msg.isWrite ? "WRITE" : "READ", msg.address, msg.pid);
        fflush(logfile);

        MessageFromOSS reply;
        reply.mtype = msg.pid;
        reply.granted = 1;

        msgsnd(msg_id, &reply, sizeof(reply) - sizeof(long), 0);

        advanceClock(&shared->clock, 0, 100);
    }

    return 0;
}
