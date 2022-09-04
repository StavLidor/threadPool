// Stav Lidor 207299785
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

//save fnc and paramter - this task add to queue
typedef struct task{
    void (*func) (void *);
    void* paramter;
}task;
// status  regular - dont call destory, FinshAll -call distory with flag 1 ,exit call destory with flag 0
enum status {Regular,FinshAll,Exit};

typedef struct thread_pool
{
    // number of therad
    int sizeOfThreads;
    // status of the theradpool according the up explanation
    enum status s;
    //the threads
    pthread_t* threads;
    // queue of tasks
    OSQueue* tasks;
    // lock for deque and add to queue
    pthread_mutex_t lockQueue;
    //for status updth
    pthread_mutex_t statusUpdth;
    // condition of wait
    pthread_cond_t conditionVar;
    // for thread cancel or join
    pthread_mutex_t finshThreads;
    // 0 - if dont distory 1- if destory
    int destoryThreads;
}ThreadPool;

ThreadPool* tpCreate(int numOfThreads);
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
