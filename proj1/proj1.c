/*
 * Subject      - POS
 * Project 1    - Ticket Algorithm
 * Author       - Tomáš Blažek (xblaze31)
 */

#define _POSIX_C_SOURCE 200809L
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


/**
 * @brief      Structure holds command line arguments
 */
struct Arguments {
    unsigned int countOfThreads;
    unsigned int criticalSectionPasses;
};

struct Arguments args;

/**
 * @brief      Function validate arguments and save them to global Argument structure.
 *
 * @param[in]  argc  The argc
 * @param      argv  The argv
 *
 * @return     0 on success,
 *             1 on invalid count of program arguments,
 *             2 on invalid type of arguments,
 *             3 on negative number arguments,
 *             4 when count of threads is set on 0.
 */
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
    args.criticalSectionPasses = (unsigned int) arg2;

    //printf("Count of threads:\t%d\nCrit. section passes:\t%d\n----------------------------\n", arg1, arg2);

    return 0;
}

/**
 * @brief      Function generates unique positive integer. Increments global variable ticketNumber.
 *
 * @return     Value of global variable ticketNumber incremented by 1
 */
int getticket(void){
    pthread_mutex_lock(&ticketMutex);
    int ticket = ticketNumber;
    ticketNumber++;
    pthread_mutex_unlock(&ticketMutex);
    return ticket;
}

/**
 * @brief      Entrance to critical section by value of ticket number.
 *
 * @param[in]  aenter  Number of ticket
 */
void await(int aenter){
    pthread_mutex_lock(&awaitMutex);
    while(aenter != workingTicket){
        pthread_cond_wait(&awaitCond, &awaitMutex);
    }
    pthread_mutex_unlock(&awaitMutex);
}

/**
 * @brief      Realase critical section for other thread with ticket higher by 1. 
 */
void advance(void){
    pthread_mutex_lock(&awaitMutex);
    workingTicket++;
    //Signalize to all threads in await condition
    pthread_cond_broadcast(&awaitCond);
    pthread_mutex_unlock(&awaitMutex);
}

/**
 * @brief      Implements work of thread. Simulates critical section passes. Used function nanosleep with interval <0s,0.5s>
 *             before enter to critical section and after critical section.
 *
 * @param      arg   Thread Id
 *
 * @return     0 on success.
 */
void *threadWork(void *arg){
    unsigned int threadId = *((unsigned int *) arg);
    unsigned int seed = (unsigned int) (time(NULL) ^ getpid() ^ pthread_self());

    unsigned int t;
    while((t = (unsigned int) getticket()) < args.criticalSectionPasses){
        //printf("PID(%d) has ticket %d and workingticket %d\n",  threadId, t, workingTicket);
        //random generator pseudo unique seed
        const struct timespec sleepTime = {0, rand_r(&seed) % (500 * 1000000)};
        // sleep between 0 to 500 nanosecs
        nanosleep(&sleepTime, NULL);

        await(t);
        //printf("Thread(%d) \thas ticket: \t%d\n", threadId, t);
        printf("%d\t(%d)\n", t, threadId);
        fflush(stdout); // print and clear stdout buffer
        advance();

        const struct timespec sleepTime2 = {0, rand_r(&seed) % (500 * 1000000)};
        // sleep between 0 to 500 nanosecs
        nanosleep(&sleepTime2, NULL);
    }

    return 0;
}

/**
 * @brief      Print help to user.
 */
void printHelp(void){
    printf("----------------------------------------------------\n");
    printf("Ticket algorithm by Tomas Blazek\n\n");
    printf("Usage:\n");
    printf("  ./proj1 numberOfThreads critSectionPasses\n\n");
    printf("  numberOfThreads   - Count of threads bigger than zero.\n");
    printf("  critSectionPasses - Count of critical section passes. Should be positive integer number.\n");
    printf("----------------------------------------------------\n");
}


int main(int argc,char *argv[]){
    if(validateArgs(argc,argv)){
        fprintf(stderr, "Error: Invalid Arguments!\n\n");
        printHelp();
        return 1;
    }

    // alloc memory for threads
    pthread_t pt[args.countOfThreads];
    int threadIds[args.countOfThreads];

    //generate threads
    for(unsigned int i = 1; i <= args.countOfThreads; i++){
        threadIds[i] = i;
        if(pthread_create(&pt[i], NULL, threadWork, &threadIds[i])){
            fprintf(stderr, "Error: Error while creation of thread.\n");
            return 1;
        }

    }

    //waiting for other threads
    for(unsigned int i = 1; i <= args.countOfThreads; i++){
        pthread_join(pt[i], NULL);
    }

    //Free resources
    pthread_mutex_destroy(&ticketMutex);
    pthread_mutex_destroy(&awaitMutex);
    pthread_cond_destroy(&awaitCond);
    return 0;
}
