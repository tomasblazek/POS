#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <zconf.h>
#include <memory.h>


pthread_mutex_t bufferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bufferCond = PTHREAD_COND_INITIALIZER;


typedef struct {
    bool end;
    char buffer[513];
} pthread_data_t;



void bufferWait(){
    pthread_mutex_lock(&bufferMutex);
    pthread_cond_wait(&bufferCond, &bufferMutex);
    pthread_mutex_unlock(&bufferMutex);
}


void bufferSignal(){
    pthread_mutex_lock(&bufferMutex);
    pthread_cond_signal(&bufferCond);
    pthread_mutex_unlock(&bufferMutex);
}


void *threadRun(void *arg){
    pthread_data_t *data = (pthread_data_t *) arg;
    //printf("Run running\n");

    char buffer[513];

    while(!data->end) {
        bufferWait();

        strcpy(buffer, data->buffer);
        printf("Running thread buffer: %s", buffer);

        bufferSignal();

        // Exit on exit command
        if(!strcmp(buffer, "exit\n")){
            return 0;
        }
    }

    return 0;
}


void *threadRead(void *arg){
    pthread_data_t *data = (pthread_data_t *) arg;

    ssize_t res;
    while(!data->end){
        printf("$ ");
        fflush(stdout); // flush $

        memset(data->buffer, 0, 513);
        res = read(0, &data->buffer, 513); // read from stdin

        if (data->buffer[512] != '\0'){
            fprintf(stderr, "Error: Input exceed maximum length of input line!\n");
            while(data->buffer[512] != '\0'){ // read rest of line
                memset(data->buffer, 0, 513);
                res = read(0, &data->buffer, 513); // read from stdin
                if(res < 0){
                    perror("Error while reading shell input!\n");
                    data->end = true;
                }
            }
            continue;
        }


        if(res < 0){
            perror("Error while reading shell input!\n");
            data->end = true;
            continue;
        }


        if(!strcmp(data->buffer, "exit\n")){
            data->end = true;
            bufferSignal();
            return 0;
        }

        // Send signal to thread Run to run
        bufferSignal();

        // Wait till buffer gonna be processed by thread Run
        bufferWait();
    }

    return 0;
}


int main() {
    printf("Hello, World!\n");

    pthread_t pthread_read;
    pthread_t pthread_run;

    pthread_data_t data;
    data.end = false;

    if(pthread_create(&pthread_read, NULL, threadRead, (void *) &data)){
        fprintf(stderr, "Error: Error while creation of Read thread.\n");
        return 1;
    }

    if(pthread_create(&pthread_run, NULL, threadRun, (void *) &data)){
        fprintf(stderr, "Error: Error while creation of Run thread.\n");
        return 1;
    }

    pthread_join(pthread_read, NULL);
    pthread_join(pthread_run, NULL);

    return 0;
}