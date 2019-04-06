#include <stdio.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>


int ticketNumber = 0;
int workingTicket = 0;


pthread_mutex_t ticketMutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct {
    unsigned int countOfThreads;
    unsigned int criticalSectionPases;
} arguments;


//TODO number of threads 0 and passes 1+
arguments* validateArgs(int argc, char *argv[]){
    // test arguments to count
    if(argc != 3){
        fprintf(stderr, "Error: Invalid count of program arguments!\n");
        return NULL;
    }

    char* ptr1;
    char* ptr2;
    int arg1 = (int) strtol(argv[1], &ptr1, 10);
    int arg2 = (int) strtol(argv[2], &ptr2, 10);

    // test arguments to int
    if(strcmp(ptr1, "") != 0 || strcmp(ptr2, "") != 0){
        fprintf(stderr, "Error: Invalid type of program arguments! Should be positive integers.\n");
        return NULL;
    }

    // test to positive integer
    if(arg1 < 0 || arg2 < 0){
        fprintf(stderr, "Error: Invalid value of arguments! Should be positive integers.\n");
        return NULL;
    }

    arguments *args = malloc(sizeof(arguments));
    if(args == NULL){
        fprintf(stderr, "Error: Malloc of arguments failed.\n");
        return NULL;
    }

    args->countOfThreads = (unsigned int) arg1;
    args->criticalSectionPases = (unsigned int) arg2;

    printf("Count of threads:\t%d\nCrit. section passes:\t%d\n----------------------------\n", arg1, arg2);

    return args;
}


int getticket(void){
    pthread_mutex_lock(&ticketMutex);
    ticketNumber++;
    pthread_mutex_unlock(&ticketMutex);
    return ticketNumber;
}

void await(int aenter){
    while(aenter != workingTicket){

    }
}
void advance(void){
    workingTicket++;
}


void *threadWork(void *arg){
    unsigned int threadId = (unsigned int) arg;
    int t = getticket();

    printf("Thread(%d) has ticket %d\n", t, threadId);

    return 0;
}

int main(int argc,char *argv[]){
    arguments *args = validateArgs(argc,argv);
    if(args == NULL){
        fprintf(stderr, "Error: Invalid Arguments!\n");
        return 1;
    }

    // alloc memory for threads
    pthread_t* pt;
    pt = (pthread_t*) malloc(args->countOfThreads*sizeof(pthread_t));

    //generate threads
    for(unsigned int i = 0; i < args->countOfThreads; i++){
        pthread_create(&pt[i], NULL, threadWork, (void*) i);
    }

    //waiting for other threads
    for(unsigned int i = 0; i < args->countOfThreads; i++){
        pthread_join(pt[i], NULL);
    }

    free(pt);
	return 0;
}
