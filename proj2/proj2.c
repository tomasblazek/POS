/*
 * Subject      - POS
 * Project 2    - Shell
 * Author       - Tomáš Blažek (xblaze31)
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <zconf.h>
#include <memory.h>
#include <malloc.h>
#include <wait.h>
#include <ctype.h>
#include <stdlib.h>


pthread_mutex_t bufferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bufferCond = PTHREAD_COND_INITIALIZER;

int get_arg();

#define BUFFER_SIZE 513

typedef struct {
    bool end;
    char buffer[BUFFER_SIZE];
} pthread_data_t;

typedef struct {
    bool background;
    int argc;
    char **argv;
    int argvSize;
    char inputFile[BUFFER_SIZE];
    char outputFile[BUFFER_SIZE];
} program_data_t;


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


char* skipWhiteSpaces(char *buffer){
    int i = 0;
    while(isspace(buffer[i]) && buffer[i] != '\0'){
        i++;
    }

    return &buffer[i];
}


// in buffer will be arg and rest is returned
char* getArg(char *buffer, char** rest) {
    int i = 0;
    bool in_quotation = false;

    while(buffer[i] != '\0' && (!isspace(buffer[i]) || in_quotation)){
        if(buffer[i] == '\"'){
            if(in_quotation){
                in_quotation = false;
            } else{
                in_quotation = true;
            }
        }
        i++;
    }

    *rest = &buffer[i];

    char* arg = malloc(sizeof(char *));
    if(arg == NULL){
        perror("malloc");
        return NULL;
    }
    strncpy(arg, buffer, (size_t) i);
    arg[i] = '\0';
    return arg;
}


int parseArgsFromBuffer(program_data_t *program_data, char* buffer){
    int i = 0;
    char *rest;

    char **argv = malloc(sizeof(char *));
    if (argv == NULL){
        perror("malloc");
        return 1;
    }

    while(strcmp(buffer, "") != 0){
        buffer = skipWhiteSpaces(buffer);
        if(!strcmp(buffer, "")){
            break;
        }
        argv[i] = getArg(buffer, &rest);
        if(argv[i] == NULL){
            for(int j = 0; j < i; j++){
                free(argv[j]);
            }
            free(argv);
            return 1;
        }
        fflush(stdout);
        buffer = rest;
        i += 1;
    }
    program_data->argc = i;
    program_data->argv = argv;

    return 0;
}

// on first white character
size_t lengthOfArgument(const char *s){
    size_t i = 0;

    if(s == NULL){
        return i;
    }

    while(!isspace(s[i]) && s[i] != '\0'){
        i++;
    }

    return i;
}


char* findRedirect(char *buffer, char c) {
    int i = 0;
    char *fileName = NULL;
    while (buffer[i] != '\0') {
        while (buffer[i] != c && buffer[i] != '\0') {
            i++;
        }

        if (buffer[i] == '\0'){
            break;
        }

        buffer[i] = ' ';
        memset(fileName, ' ', lengthOfArgument(fileName));

        fileName = skipWhiteSpaces(&buffer[i + 1]);
        i++;
    }

    return fileName;
}


void freeResources(program_data_t *data){
    for(int i = 0; i < (* data).argc; i++){
        free((* data).argv[i]);
    }
    free((* data).argv);
}



void *threadRun(void *arg){
    pthread_data_t *pthread_data = (pthread_data_t *) arg;

    char buffer[BUFFER_SIZE];
    program_data_t program_data;

    while(!pthread_data->end) {
        bufferWait();

        strcpy(buffer, pthread_data->buffer);
        printf("Buffer: %s", buffer);

        // init program_data pthread_data structure
        program_data.background = false;
        memset(program_data.inputFile, 0, sizeof(program_data.inputFile));
        memset(program_data.outputFile, 0, sizeof(program_data.outputFile));

        // load input file and clear it from buffer
        char *inputFile = findRedirect(buffer, '<');
        strncpy(program_data.inputFile, inputFile, lengthOfArgument(inputFile));
        memset(inputFile, ' ', lengthOfArgument(inputFile));

        printf("INPUT: %s\n", program_data.inputFile);

        // load output file and clear it from buffer
        char *outputFile = findRedirect(buffer, '>');
        strncpy(program_data.outputFile, outputFile, lengthOfArgument(outputFile));
        memset(outputFile, ' ', lengthOfArgument(outputFile));

        printf("OUTPUT: %s\n", program_data.outputFile);

        if(parseArgsFromBuffer(&program_data, buffer)){
            pthread_data->end = true;
            break;
        }

        printf("Argc: %d\n", program_data.argc);
        for(int i = 0; i < program_data.argc; i++){
            printf("Argv(%d): %s\n", i, program_data.argv[i]);
        }
        fflush(stdout);

        // Exit on exit command
        if(!strcmp(program_data.argv[0], "exit")){
            pthread_data->end = true;
            bufferSignal();
            break;
        }

        int pid = fork();
        if(pid==-1) {
            perror("fork failure");
            continue;
        } else if(pid > 0){
            //parent
            if (!program_data.background){
                waitpid(pid, NULL, 0);
            }
        } else {
            //child
            execvp(program_data.argv[0], program_data.argv);
            perror("execvp");
            exit(1);
        }

        freeResources(&program_data);

        bufferSignal();
    }

    freeResources(&program_data);
    return 0;
}


void *threadRead(void *arg){
    pthread_data_t *data = (pthread_data_t *) arg;

    ssize_t ret;
    while(!data->end){
        printf("$ ");
        fflush(stdout); // flush $

        memset(data->buffer, 0, BUFFER_SIZE);
        ret = read(0, &data->buffer, BUFFER_SIZE); // read from stdin

        if (data->buffer[BUFFER_SIZE-1] != '\0'){
            fprintf(stderr, "Error: Input exceed maximum length of input line!\n");
            while(data->buffer[BUFFER_SIZE-1] != '\0'){ // read rest of line
                memset(data->buffer, 0, BUFFER_SIZE);
                ret = read(0, &data->buffer, BUFFER_SIZE); // read from stdin
                if(ret < 0){
                    perror("Error while reading shell input!\n");
                    data->end = true;
                }
            }
            continue;
        }


        if(ret < 0){
            perror("Error while reading shell input!\n");
            data->end = true;
            continue;
        }


        // Send signal to thread Run to run
        bufferSignal();

        // Wait till buffer gonna be processed by thread Run
        bufferWait();
    }

    return 0;
}


int main(int argc, char *argv[]) {
    printf("Hello, World!\n");

    printf("Argc: %d\n", argc);
    for(int i = 0; i < argc; i++){
        printf("Argv(%d): %s\n", i, argv[i]);
    }

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