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
#include <fcntl.h>


pthread_mutex_t bufferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bufferCond = PTHREAD_COND_INITIALIZER;

#define BUFFER_SIZE 513

typedef struct {
    bool end;
    char buffer[BUFFER_SIZE];
    pid_t running_process_pid;
} pthread_data_t;

typedef struct {
    bool background;
    int argc;
    char **argv;
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

    char **argv = malloc(sizeof(char **));
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
        buffer = rest;
        i += 1;
    }
    argv[i] = NULL;

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

void execute(program_data_t *program_data){
    int inputFile, outputFile;
    if (strlen(program_data->inputFile) > 0){
        inputFile = open(program_data->inputFile, O_RDONLY);
        if(inputFile < 0) {
            perror("open input");
            exit(2);
        }

        if(dup2(inputFile, 0) < 0){
            perror("dup2");
            exit(3);
        }
    }

    if (strlen(program_data->outputFile) > 0){
        outputFile = open(program_data->outputFile, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        if(outputFile < 0) {
            perror("open input");
            exit(2);
        }

        if(dup2(outputFile, 1) < 0){
            perror("dup2");
            exit(3);
        }
    }

    execvp((*program_data).argv[0], (*program_data).argv);
    perror("execvp");

    if (strlen(program_data->inputFile) > 0) {
        close(inputFile);
    }

    if (strlen(program_data->outputFile) > 0) {
        close(outputFile);
    }
}


void *threadRun(void *arg){
    pthread_data_t *pthread_data = (pthread_data_t *) arg;

    char buffer[BUFFER_SIZE];
    program_data_t program_data;

    while(!pthread_data->end) {
        bufferWait();

        strcpy(buffer, pthread_data->buffer);

        // init program_data pthread_data structure
        program_data.background = false;
        memset(program_data.inputFile, 0, sizeof(program_data.inputFile));
        memset(program_data.outputFile, 0, sizeof(program_data.outputFile));

        // search for run on background & char
        char *ampersand = strchr(buffer, '&');
        if (ampersand != NULL){
            program_data.background = true;
            ampersand[0] = '\0';
        }

        // load input file and clear it from buffer
        char *inputFile = findRedirect(buffer, '<');
        strncpy(program_data.inputFile, inputFile, lengthOfArgument(inputFile));
        memset(inputFile, ' ', lengthOfArgument(inputFile));

        // load output file and clear it from buffer
        char *outputFile = findRedirect(buffer, '>');
        strncpy(program_data.outputFile, outputFile, lengthOfArgument(outputFile));
        memset(outputFile, ' ', lengthOfArgument(outputFile));

        // load program arguments
        if(parseArgsFromBuffer(&program_data, buffer)){
            pthread_data->end = true;
            break;
        }


        if(program_data.argc > 0) {
            // Exit on exit command
            if(!strcmp(program_data.argv[0], "exit")){
                pthread_data->end = true;
                bufferSignal();
                break;
            }

            int pid = fork();
            if (pid == -1) {
                perror("fork failure");
                continue;
            } else if (pid > 0) {
                //parent
                if (!program_data.background) {
                    pthread_data->running_process_pid = pid;
                    waitpid(pid, NULL, 0);
                    pthread_data->running_process_pid = 0;
                }
            } else {
                //child
                execute(&program_data);
                exit(1);
            }
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


int main() {
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