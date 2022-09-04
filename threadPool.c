// Stav Lidor 207299785
#include "threadPool.h"


/******************
* Function Name:fncTh
* Input:pointer to threadPool
* Output: return 0 there not have error in runnig else -1.
* Function Operation: the func that all the thread in the thread pool run.
* for run function in the queue or wait if the queue empty that have new task.
******************/
void* fncTh(void *arg){
    int flag =1;
    ThreadPool* tp = (ThreadPool*) arg;
    while(1){
        int isFinsh=0,isEmptyOueue=0;
        // lock to know the status
        if(pthread_mutex_lock(&tp->statusUpdth)!=0){
            perror("Error in mutex lock");
              return ((void*) -1);
        }
        if(tp->s ==Exit){
            isFinsh=1;
        }
        if(pthread_mutex_unlock(&tp->statusUpdth)!=0){
                perror("Error in mutex unlock");
                    return ((void*) -1);
        }
        if(isFinsh)
            break;
        task* t=NULL;
         // dequeue new task
        if(pthread_mutex_lock(&tp->lockQueue)!=0){
              perror("Error in mutex lock");
              return ((void*) -1);
        }
        if(tp->tasks != NULL && !osIsQueueEmpty(tp->tasks)){
             t = (task*) osDequeue(tp->tasks);
        } 
        else
            isEmptyOueue=1;
       if(pthread_mutex_unlock(&tp->lockQueue) != 0){
           perror("Error in mutex unlock");
           if(t!=NULL)
                free(t);
              return ((void*) -1);;
       }
       if(!isEmptyOueue){
                // if have task
            if(t!=NULL){
                //run the task
                t->func(t->paramter);
                // free the task
                free(t);
            }
            continue;
       }
        int toWait=0;
        if(pthread_mutex_lock(&tp->statusUpdth)!=0){
            perror("Error in mutex lock");
                return ((void*) -1);
        }
            if(tp->s ==Regular){
                toWait=1;
            }
        if(pthread_mutex_unlock(&tp->statusUpdth)!=0){
                perror("Error in mutex unlock");
                    return ((void*) -1);
        }
        if(!toWait){
            break;
        }
        // if the queue empty and dont destory tp wait to new task to add the queue
        if(pthread_mutex_lock(&tp->lockQueue)!=0){
            perror("Error in mutex lock");
            return ((void*) -1);
        }
        // if destory the queue
        if(tp->tasks == NULL){
           if(pthread_mutex_unlock(&tp->lockQueue)!=0){}
            return ((void*) -1);
        }
        while(tp->tasks!=NULL&&osIsQueueEmpty(tp->tasks) &&tp->s ==Regular){
            // its wait until new task add the queue or the status chenge when tp destory
            if(pthread_cond_wait(&tp->conditionVar,&tp->lockQueue)!=0){
                perror("Error in cond wait");
                if(pthread_mutex_unlock(&tp->lockQueue)!=0){
                        return ((void*) -1);
                    }
                return ((void*) -1);
            }
        }
        if(pthread_mutex_unlock(&tp->lockQueue)!=0){
            perror("Error in mutex unlock");
            return ((void*) -1);
        }
        
        
    }
    return ((void*) 0);
}

/******************
* Function Name: errorCloser
* Input: the threadpool, and cause of error messge,cancelThreads>0 - if to cancel the threads,
* destoryQueue>0 - if need destory the queue,skipWaiting - if need to skip Wating threads, updthStatus>0-if need to updth  that is exit
* if somting of cancelThreads, destoryQueue,skipWaiting,updthStatus is -1  it beacuse care about him before.
* Output: none
* Function Operation: close the thing could to close if and if dont be tpDestory exit -1.
******************/
void errorCloser(ThreadPool* threadPool, char * messge,int cancelThreads, int destoryQueue,int skipWaiting,int updthStatus){
    int error=0,doNothing=0;
    // if not alardy print error
    if(messge != NULL){
        perror(messge);
    }
    // in destory.
    if( updthStatus!= -1 && threadPool->s != Regular){
               return;
     } 
    int waiting = skipWaiting;
    // if can updth status
    if(updthStatus>0){
        if(pthread_mutex_lock(&threadPool->statusUpdth)!=0){}
        else{
            // if need wakeup waiting
            if( threadPool->s == Regular){
                waiting = 1;
            } 
            else{
                doNothing=1;
            }
            threadPool->s= Exit;
            if(pthread_mutex_unlock(&threadPool->statusUpdth)!=0){
                error=1;
            }
        }
    }
    if(updthStatus!= -1 && doNothing == 1){
        return;
    }
    // if can destory queue
    if(!error && destoryQueue>0){
        //free all the left task in the queue
        if(pthread_mutex_lock(&threadPool->lockQueue)!=0){
            error=1;
        }
        if(!error && waiting && pthread_cond_broadcast(&threadPool->conditionVar)!=0){}
        if(threadPool->tasks != NULL && !error){
            while(!osIsQueueEmpty(threadPool->tasks)){
                task* t = (task*) osDequeue(threadPool->tasks);
                if(t)
                    free(t);
            }
            // destory the queue
            osDestroyQueue(threadPool->tasks);
            threadPool->tasks= NULL;
        }
        if(!error && pthread_mutex_unlock(&threadPool->lockQueue)!=0){
            error=1;
        }

    }
    int j;
    int isFreeThareads=1;
     // if can cancels threads.
    if(!error && cancelThreads>0){
        if(pthread_mutex_lock(&threadPool->finshThreads)!=0){
            error=1;
        }
        //if destory all threads
        if(threadPool->destoryThreads==1){
            if(!error && pthread_mutex_unlock(&threadPool->finshThreads)!=0){}
            return;
        }
        if(!error){
            for(j=0;j<threadPool->sizeOfThreads;j++){
                    if(pthread_join(threadPool->threads[j],NULL)!=0){
                        isFreeThareads=0;
                    }
            }
            threadPool->destoryThreads=1;
            if(isFreeThareads){
                    free(threadPool->threads);
            }
            if(pthread_mutex_unlock(&threadPool->finshThreads)!=0){error=1;}
            if (!error&&isFreeThareads)
            {
                //destory all mutexs
                if(pthread_mutex_destroy(&threadPool->lockQueue)!=0 ||pthread_cond_destroy(&threadPool->conditionVar)!=0
                ||pthread_mutex_destroy(&threadPool->statusUpdth)!=0||pthread_mutex_destroy(&threadPool->finshThreads)){
                }
            }

        }
    }
   
    free(threadPool);
    exit(-1);

}
/******************
* Function Name: tpCreate
* Input:number of thread should be in the threadpool
* Output: pointer to thread pool
* Function Operation: the func that create the thread pool
******************/
ThreadPool* tpCreate(int numOfThreads){
    // malloc to thread pool
    ThreadPool* tp = (ThreadPool*) malloc(sizeof(ThreadPool));
    if (tp == NULL){
        perror("Error in: malloc");
        exit(-1);
    }
    // intilize lock 
    if(pthread_mutex_init(&tp->finshThreads, NULL) != 0){
        perror("Error in mutex init");
        free(tp);
        exit(-1); 

    }
    // intilize lock 
    if(pthread_mutex_init(&tp->statusUpdth, NULL) != 0){
        perror("Error in mutex init");
        pthread_mutex_destroy(&tp->finshThreads);
        free(tp);
        exit(-1); 

    }
    // intilize conditon
    if(pthread_cond_init(&tp->conditionVar,NULL) !=0){
        perror("Error in mutex cond init");
        pthread_mutex_destroy(&tp->statusUpdth);
        pthread_mutex_destroy(&tp->finshThreads);
        free(tp);
        exit(-1); 

    }
     // intilize lock 
    if (pthread_mutex_init(&tp->lockQueue, NULL) != 0)
    {
        perror("Error in mutex init");
        pthread_mutex_destroy(&tp->statusUpdth);
        pthread_cond_destroy(&tp->conditionVar);
        pthread_mutex_destroy(&tp->finshThreads);
          free(tp);
        exit(-1); 
    }
    //intilize queue for tasks
    tp->tasks = osCreateQueue();
    if(tp->tasks == NULL)  {
        perror("Error in: malloc");
        pthread_mutex_destroy(&tp->statusUpdth);
        pthread_mutex_destroy(&tp->lockQueue);
        pthread_cond_destroy(&tp->conditionVar);
        pthread_mutex_destroy(&tp->finshThreads);
        free(tp);
        exit(-1);  
    }
      //intilize threads according the number in the input of this function
    tp->threads =(pthread_t*) malloc(sizeof(pthread_t)*numOfThreads);
    if(tp->threads == NULL){
        perror("Error in: malloc");
        osDestroyQueue(tp->tasks);
        pthread_mutex_destroy(&tp->lockQueue);
        pthread_mutex_destroy(&tp->statusUpdth);
        pthread_cond_destroy(&tp->conditionVar);
        pthread_mutex_destroy(&tp->finshThreads);
        free(tp);
        exit(-1); 
    }
    int i; 
     tp->s = Regular;
    tp->sizeOfThreads = numOfThreads; 
    tp->destoryThreads= 0;
    for(i=0;i<numOfThreads;i++){
        int  err=0;
        err = pthread_create(&tp->threads[i], NULL, fncTh,tp);
        if (err){
            int errorUnlock=0;
            perror("Error in pthread create");
            int j,notFree=0;
            if(pthread_mutex_lock(&tp->statusUpdth)!=0){}
            else{
                tp->s= Exit;
                if(pthread_mutex_unlock(&tp->statusUpdth)!=0){
                    errorUnlock=1;
                }
            }
            if(!errorUnlock && pthread_mutex_lock(&tp->lockQueue)!=0){}
            else{
                if(pthread_cond_broadcast(&tp->conditionVar)!=0){}
                osDestroyQueue(tp->tasks);
                tp->tasks=NULL;
                if(pthread_mutex_unlock(&tp->lockQueue)!=0){
                    errorUnlock=1;
                }
            }
            for(j=0;j<i;j++){
                if(pthread_join(tp->threads[j],NULL)!=0){
                    notFree=1;
                }
            }
            if(!notFree){
                free(tp->threads);
                pthread_mutex_destroy(&tp->statusUpdth);
                pthread_mutex_destroy(&tp->lockQueue);
                pthread_cond_destroy(&tp->conditionVar);
                pthread_mutex_destroy(&tp->finshThreads);
            }
            free(tp);
            exit(-1);
        }
            
    }
   
    return tp;
}



/******************
* Function Name: tpDestroy
* Input: the threadpool and number that is flag - if should to finsh all the tasks in queue or not
* Output: none
* Function Operation:destory the thread pool free all the mempry in heap
******************/
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){
    int isDestory=0;
    if(pthread_mutex_lock(&threadPool->statusUpdth)!=0){
         errorCloser(threadPool,"Error in mutex lock",1,1,1,0);
    }
    if(threadPool->s !=Regular){
        isDestory=1;
    }
    // updth status
    else if(shouldWaitForTasks){
        // the status finsh the tasks that left in queue
        threadPool->s = FinshAll;
    }
    else{
        // dont finsh the task that left in queue
         threadPool->s = Exit;
         
    }
    if(pthread_mutex_unlock(&threadPool->statusUpdth)!=0){
            if(isDestory)
                return;
             errorCloser(threadPool,"Error in mutex unlock",0,0,0,-1);
    }
    if(isDestory)
        return;
     int i,err=0;
     if(pthread_mutex_lock(&threadPool->lockQueue)!=0){
         errorCloser(threadPool,"Error in mutex lock",1,0,0,-1);
        }
        if(threadPool->s == Exit){
             while(!osIsQueueEmpty(threadPool->tasks)){
                task* t = (task*) osDequeue(threadPool->tasks);
                if(t)
                free(t);
            }
        }
     // updth the thereds that wait that the status change
    if(pthread_cond_broadcast(&threadPool->conditionVar)!=0){
        int errLock=0;
        if(pthread_mutex_unlock(&threadPool->lockQueue)!=0){
            errLock=1;
        }
        errorCloser(threadPool,"Error in broadcast",!errLock,!errLock,0,-1);
    }
    if(pthread_mutex_unlock(&threadPool->lockQueue)!=0){
             errorCloser(threadPool,"Error in mutex unlock",0,0,0,-1);
    }
    int errInJoin=0;
    if(pthread_mutex_lock(&threadPool->finshThreads)!=0){
          errorCloser(threadPool,"Error in mutex lock",0,1,0,-1);
    }
    // all close by error in other thread
    if(threadPool->destoryThreads==1){
        if(pthread_mutex_unlock(&threadPool->finshThreads)!=0){}
       return;
    }
    threadPool->destoryThreads=1;
    
   // join all the thread
    for(i=0;i<threadPool->sizeOfThreads;i++){
       void * retVal;
        if(pthread_join(threadPool->threads[i],&retVal)!=0){
            if(errInJoin!=1){
                perror("Error in pthread join");
                errInJoin=1;
            } 
        }
        // if system call faild in threadPool
        else if(retVal == (void *)-1){
                err =1;
        }
             
    }
    if(pthread_mutex_unlock(&threadPool->finshThreads)!=0){
            if(!err){
             perror("Error in mutex unlock");
              errorCloser(threadPool,NULL,0,0,0,-1);
            }        
    }
     // close - free all
    if(err || errInJoin){
        if(!errInJoin){
            free(threadPool->threads);
            errorCloser(threadPool,NULL,-1,-1,0,-1);
        }  
         errorCloser(threadPool,NULL,0,1,0,-1);
    } 
    else{
        // destory the queue
        osDestroyQueue(threadPool->tasks);
        free(threadPool->threads);
        //destory all mutexs
        if(pthread_mutex_destroy(&threadPool->lockQueue)!=0 ||pthread_cond_destroy(&threadPool->conditionVar)!=0
            ||pthread_mutex_destroy(&threadPool->statusUpdth)!=0||pthread_mutex_destroy(&threadPool->finshThreads)){
                    perror("Error in mutex destroy");
                    err=1;
            }
        free(threadPool);
        if(err==1)
            exit(-1);
    }
        
}

/******************
* Function Name: tpInsertTask
* Input: the threadpool ,fnc to insert to queue, param of task
* Output: return -1 if its call after destory else 0
* Function Operation:insert new task to theadpool
******************/
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param){
    int flag =0;
    if(pthread_mutex_lock(&threadPool->statusUpdth)!=0){
         errorCloser(threadPool,"Error in mutex lock",1,1,1,0);
          // not do exit beacuse it call before tpdestory
         return -1;
    }
    // if destory the thread pool dont need to insert a new task
     if(threadPool->s != Regular){
        flag =1;
    }
    if(pthread_mutex_unlock(&threadPool->statusUpdth)!=0){
             errorCloser(threadPool,"Error in mutex unlock",0,0,0,0);
             // not do exit beacuse it call before tpdestory
             flag =1;
    }
    if(flag==1){
        return -1;
    }
    // save the new task
    task* t=(task*) malloc(sizeof(task));
    t->func = computeFunc;
    t->paramter =param;
    //insert to queue new task
    if(pthread_mutex_lock(&threadPool->lockQueue)!=0){
        errorCloser(threadPool,"Error in mutex lock",1,0,0,1);
        // not do exit beacuse it call before tpdestory
        return -1;
    }
    osEnqueue(threadPool->tasks,t);
    // updth the therad that wait ,that have a new task
    if(pthread_cond_signal(&threadPool->conditionVar)!=0){
        int isUnlockError=0;
        if(pthread_mutex_unlock(&threadPool->lockQueue)!=0){
            isUnlockError=1;
        }
        errorCloser(threadPool,"Error in cond signal",!isUnlockError,!isUnlockError,0,!isUnlockError);
        // not do exit beacuse it call before tpdestory
        flag=-1;
    }
    if(pthread_mutex_unlock(&threadPool->lockQueue)!=0){
        errorCloser(threadPool,"Error in mutex unlock",0,0,0,0);
        // not do exit beacuse it call before tpdestory
        flag=-1;
    }
    return flag;
}