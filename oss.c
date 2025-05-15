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
#define MSGKEY 1234
#define SHMKEY 5678
#define BILLION 1000000000
#define DISK_IO_TIME_NS 14000000

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

int shm_id = -1, msg_id = -1;
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

int findLRUFrame() {
    int lruIndex = -1;
    SimClock oldest = { .seconds = __INT_MAX__, .nanoseconds = __INT_MAX__ };
    for (int i = 0; i < FRAME_TABLE_SIZE; i++) {
        if (shared->frameTable[i].occupied) {
            SimClock ref = shared->frameTable[i].lastRef;
            if (ref.seconds < oldest.seconds ||
                (ref.seconds == oldest.seconds && ref.nanoseconds < oldest.nanoseconds)) {
                oldest = ref;
                lruIndex = i;
            }
        }
    }
    return lruIndex;
}

int findFreeFrame() {
    for (int i = 0; i < FRAME_TABLE_SIZE; i++) {
        if (!shared->frameTable[i].occupied) return i;
    }
    return -1;
}

void logMemoryLayout() {
    fprintf(logfile, "Current memory layout at time %d:%d is:\n", shared->clock.seconds, shared->clock.nanoseconds);
    for (int i = 0; i < FRAME_TABLE_SIZE; i++) {
        Frame *f = &shared->frameTable[i];
        fprintf(logfile, "Frame %d: %s Dirty=%d LastRef=%d:%d PID=%d Page=%d\n",
                i, f->occupied ? "Yes" : "No", f->dirtyBit, f->lastRef.seconds, f->lastRef.nanoseconds,
                f->pid, f->pageNumber);
    }
    for (int p = 0; p < MAX_PROCESSES; p++) {
        fprintf(logfile, "P%d page table: ", p);
        for (int j = 0; j < NUM_PAGES; j++) {
            fprintf(logfile, "%d ", shared->pageTables[p].frameNumber[j]);
        }
        fprintf(logfile, "\n");
    }
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
    memset(shared, -1, sizeof(SharedData));

    msg_id = msgget(MSGKEY, IPC_CREAT | 0666);
    if (msg_id < 0) {
        perror("msgget");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == 0) {
        execl("./user_proc", "user_proc", NULL);
        perror("execl");
        exit(1);
    }

    while (1) {
        MessageToOSS msg;
        if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1) continue;

        int page = msg.address / PAGE_SIZE;
        int procIdx = msg.pid % MAX_PROCESSES;
        int frame = shared->pageTables[procIdx].frameNumber[page];

        if (frame != -1 && shared->frameTable[frame].pid == msg.pid && shared->frameTable[frame].pageNumber == page) {
            fprintf(logfile, "oss: PID %d %s address %d (page %d) HIT in frame %d at time %d:%d\n",
                    msg.pid, msg.isWrite ? "WRITE" : "READ", msg.address, page, frame,
                    shared->clock.seconds, shared->clock.nanoseconds);

            if (msg.isWrite) shared->frameTable[frame].dirtyBit = 1;
            shared->frameTable[frame].lastRef = shared->clock;
        } else {
            fprintf(logfile, "oss: PID %d %s address %d (page %d) PAGE FAULT at time %d:%d\n",
                    msg.pid, msg.isWrite ? "WRITE" : "READ", msg.address, page,
                    shared->clock.seconds, shared->clock.nanoseconds);

            int free = findFreeFrame();
            if (free == -1) free = findLRUFrame();

            if (shared->frameTable[free].dirtyBit) {
                fprintf(logfile, "oss: Evicting dirty frame %d, adding disk I/O time\n", free);
                advanceClock(&shared->clock, 0, DISK_IO_TIME_NS);
            } else {
                fprintf(logfile, "oss: Evicting frame %d\n", free);
            }

            shared->frameTable[free].occupied = 1;
            shared->frameTable[free].dirtyBit = msg.isWrite;
            shared->frameTable[free].pid = msg.pid;
            shared->frameTable[free].pageNumber = page;
            shared->frameTable[free].lastRef = shared->clock;
            shared->pageTables[procIdx].frameNumber[page] = free;
        }

        MessageFromOSS reply;
        reply.mtype = msg.pid;
        reply.granted = 1;
        msgsnd(msg_id, &reply, sizeof(reply) - sizeof(long), 0);

        advanceClock(&shared->clock, 0, 100);

        if (shared->clock.nanoseconds % BILLION < 1000) logMemoryLayout();
    }

    return 0;
}
