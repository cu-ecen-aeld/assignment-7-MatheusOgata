#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    struct thread_data* tData = (struct thread_data*) (thread_param);

    DEBUG_LOG("sleep for %d to lock mutex\n", tData->wait_to_obtain_ms);
    usleep(tData->wait_to_obtain_ms * 1000);

    if( pthread_mutex_lock(tData->mutex) != 0)
    {
       ERROR_LOG("It was not possible to get mutex");
       tData->thread_complete_success = false;
       return NULL;
    }
    else
    {
       DEBUG_LOG("sleep for %d to unlock mutex\n", tData->wait_to_release_ms);
       usleep(tData->wait_to_release_ms * 1000);
    }

    if(pthread_mutex_unlock(tData->mutex) != 0)
    {
       ERROR_LOG("It was not possible to release mutex");
       tData->thread_complete_success = false;
       return NULL;
    }
    else
    {
       DEBUG_LOG("mutex unlocked: thread completed successfully\n");
    }

    tData->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     *  allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    bool result = false;
    struct thread_data* tData = (struct thread_data*)malloc(sizeof(struct thread_data));

    if( tData == NULL)
    {
	ERROR_LOG("there's no more memory");
	return result;
    }
 
    tData->mutex = mutex;
    tData->wait_to_obtain_ms = wait_to_obtain_ms;
    tData->wait_to_release_ms = wait_to_release_ms;  
    tData->thread_complete_success = false;


    if(pthread_create(thread, NULL, threadfunc, (void *)tData) != 0)
    {	
       DEBUG_LOG("FAILURE: thread was not created\n");
       result = false;
       free((void *) tData);
    }
    else
    {
       DEBUG_LOG("Thread created successfully\n");
       result = true;
    }

    return result;
}

