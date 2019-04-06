/*
 * Subject      - POS
 * Project 1    - Ticket Algorithm
 * Author       - Tomáš Blažek (xblaze31)
 */


#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zconf.h>


int ticketNumber = 0 ;
int workingTicket = 0;


pthread_mutex_t ticketMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t awaitMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t awaitCond = PTHREAD_COND_INITIALIZER;



struct Arguments {
    unsigned int countOfThreads;
    unsigned int criticalSectionPases;
};

struct Arguments args;

//TODO number of threads 0 and passes 1+
int validateArgs(int argc, char *argv[]){
    // test arguments to count
    if(argc != 3){
        fprintf(stderr, "Error: Invalid count of program arguments!\n");
        return 1;
    }

    char* ptr1;
    char* ptr2;
    int arg1 = (int) strtol(argv[1], &ptr1, 10);
    int arg2 = (int) strtol(argv[2], &ptr2, 10);

    // test arguments to int
    if(strcmp(ptr1, "") != 0 || strcmp(ptr2, "") != 0){
        fprintf(stderr, "Error: Invalid type of program arguments! Should be positive integers.\n");
        return 2;
    }

    // test to positive integer
    if(arg1 < 0 || arg2 < 0){
        fprintf(stderr, "Error: Invalid value of arguments! Should be positive integers.\n");
        return 3;
    }

    if(arg1 == 0){
        fprintf(stderr, "Error: Count of threads should be more than zero.\n");
        return 4;
    }

    args.countOfThreads = (unsigned int) arg1;
    args.criticalSectionPases = (unsigned int) arg2;

    //printf("Count of threads:\t%d\nCrit. section passes:\t%d\n----------------------------\n", arg1, arg2);

    return 0;
}


int getticket(void){
    pthread_mutex_lock(&ticketMutex);
    int ticket = ticketNumber;
    ticketNumber++;
    pthread_mutex_unlock(&ticketMutex);
    return ticket;
}

void await(int aenter){
    pthread_mutex_lock(&awaitMutex);
    while(aenter != workingTicket){
        pthread_cond_wait(&awaitCond, &awaitMutex);
    }
    pthread_mutex_unlock(&awaitMutex);
}
void advance(void){
    pthread_mutex_lock(&awaitMutex);
    workingTicket++;
    //Signalize to all threads in await condition
    pthread_cond_broadcast(&awaitCond);
    pthread_mutex_unlock(&awaitMutex);
}


void *threadWork(void *arg){
    unsigned int threadId = (unsigned int) (uintptr_t) arg;
    unsigned int seed = (unsigned int) (time(NULL) ^ getpid() ^ pthread_self());

    unsigned int t;
    while((t = (unsigned int) getticket()) < args.criticalSectionPases){
        //printf("PID(%d) has ticket %d and workingticket %d\n",  threadId, t, workingTicket);
        //random generator pseudo unique seed
        const struct timespec sleepTime = {0, rand_r(&seed) % 500};
        // sleep between 0 to 500 nanosecs
        nanosleep(&sleepTime, NULL);

        await(t);
        printf("Thread(%d) \thas ticket: \t%d\n", threadId, t);
        fflush(stdout);
        advance();

        const struct timespec sleepTime2 = {0, rand_r(&seed) % 500};
        // sleep between 0 to 500 nanosecs
        nanosleep(&sleepTime2, NULL);
    }

    return 0;
}

int main(int argc,char *argv[]){
    if(validateArgs(argc,argv)){
        fprintf(stderr, "Error: Invalid Arguments!\n");
        return 1;
    }

    // alloc memory for threads
    pthread_t* pt;
    pt = (pthread_t*) malloc(args.countOfThreads*sizeof(pthread_t));


    //generate threads
    for(unsigned int i = 1; i < args.countOfThreads; i++){
        pthread_create(&pt[i], NULL, threadWork, (void*) (uintptr_t) i);

    }

    //waiting for other threads
    for(unsigned int i = 1; i < args.countOfThreads; i++){
        pthread_join(pt[i], NULL);
    }

    free(pt);
	return 0;
}
