// user_proc.c
// Author: Curtis Been
// Description: Simulates memory requests to oss

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <time.h>

#define MSGKEY 1234
#define SHMKEY 5678
#define PAGE_SIZE 1024
#define NUM_PAGES 32

typedef struct {
    int seconds;
    int nanoseconds;
} SimClock;

typedef struct {
    SimClock clock;
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

int main() {
    srand(getpid() ^ time(NULL));

    printf("user_proc (PID %d) starting\n", getpid());

    int shm_id = shmget(SHMKEY, sizeof(SharedData), 0666);
    if (shm_id < 0) {
        perror("shmget");
        exit(1);
    }

    SharedData *shared = (SharedData *)shmat(shm_id, NULL, 0);
    if (shared == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    int msg_id = msgget(MSGKEY, 0666);
    if (msg_id < 0) {
        perror("msgget");
        exit(1);
    }

    int memoryOps = 0;
    while (1) {
        int page = rand() % NUM_PAGES;
        int offset = rand() % PAGE_SIZE;
        int address = page * PAGE_SIZE + offset;
        int isWrite = rand() % 10 < 3;

        MessageToOSS msg;
        msg.mtype = 1;
        msg.pid = getpid();
        msg.address = address;
        msg.isWrite = isWrite;

        printf("user_proc PID %d sending request for address %d (%s)\n", getpid(), address, isWrite ? "WRITE" : "READ");

        if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            break;
        }

        MessageFromOSS reply;
        if (msgrcv(msg_id, &reply, sizeof(reply) - sizeof(long), getpid(), 0) == -1) {
            perror("msgrcv");
            break;
        }

        printf("user_proc PID %d received reply, granted = %d\n", getpid(), reply.granted);

        memoryOps++;
        if (memoryOps >= 1000 + (rand() % 201)) break;
    }

    printf("user_proc PID %d terminating\n", getpid());
    return 0;
}
