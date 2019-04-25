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


pthread_mutex_t pidMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bufferMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bufferCond = PTHREAD_COND_INITIALIZER;

#define BUFFER_SIZE 513


typedef struct {
    bool end;
    char buffer[BUFFER_SIZE];
    pid_t runningProcessPid;
} global_data_t;

global_data_t global_data;

typedef struct {
    bool background;
    int argc;
    char **argv;
    char inputFile[BUFFER_SIZE];
    char outputFile[BUFFER_SIZE];
} program_data_t;

/**
 * @brief Wait on condition till signal is received, then continue to buffer monitor.
 */
void bufferWait(){
    pthread_mutex_lock(&bufferMutex);
    pthread_cond_wait(&bufferCond, &bufferMutex);
    pthread_mutex_unlock(&bufferMutex);
}

/**
 * @brief Send condition signal to unlock buffer monitor.
 */
void bufferSignal(){
    pthread_mutex_lock(&bufferMutex);
    pthread_cond_signal(&bufferCond);
    pthread_mutex_unlock(&bufferMutex);
}

/**
 * @brief Jump on the first nonspace character.
 *
 * @param buffer Buffer ended with '\0' symbol
 * @return Pointer on non space character
 */
char* skipWhiteSpaces(char *buffer){
    int i = 0;
    while(isspace(buffer[i]) && buffer[i] != '\0'){
        i++;
    }

    return &buffer[i];
}

/**
 * @brief Get argument from buffer.
 *
 * @param buffer Buffer ended with '\0' symbol
 * @param rest Pointer on first char after argument end
 * @return Argument
 */
char* getArg(char *buffer, char** rest) {
    int i = 0;
    bool inQuotation = false;

    while(buffer[i] != '\0' && (!isspace(buffer[i]) || inQuotation)){
        if(buffer[i] == '\"'){
            if(inQuotation){
                inQuotation = false;
            } else{
                inQuotation = true;
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


/**
 * @brief Parse buffer and fill program data structure with arguments (set argv and argc).
 *
 * @param programData Program data struture
 * @param buffer Buffer ended with '\0' symbol
 * @return Zero on success
 */
int parseArgsFromBuffer(program_data_t *programData, char* buffer){
    const int ALLOC_INCREMENT = 10;
    int i = 0;
    int allocSize = ALLOC_INCREMENT;
    char *rest;

    char **argv = malloc(sizeof(char *) * allocSize);
    if (argv == NULL){
        perror("malloc");
        return 1;
    }

    while(strcmp(buffer, "") != 0){
        buffer = skipWhiteSpaces(buffer);
        if(!strcmp(buffer, "")){
            break;
        }

        // realloc for
        if (i + 1 >= allocSize) { // +1 because of last NULL arg
            allocSize += ALLOC_INCREMENT; // increment of alloc size
            void *new = realloc(argv, sizeof(char *) * allocSize);
            if (new == NULL) {
                perror("realloc");
                for (int j = 0; j < i; j++) {
                    free(argv[j]);
                }
                free(argv);
                return 1;
            }
            argv = new;
        }

        // assign new argument
        argv[i] = getArg(buffer, &rest);
        // free args on fail
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

    programData->argc = i;
    programData->argv = argv;

    return 0;
}

/**
 * @brief Get length of argument terminated with white space or '\0' symbol
 *
 * @param s Buffer ended with '\0' symbol
 * @return  Size of argument
 */
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

/**
 * @brief Find redirection tags and filenames in buffer and replace it with spaces (last filename is not replaced)
 *
 * @param buffer Buffer ended with '\0' symbol
 * @param c Search character
 * @return Pointer to filename after last redirect symbol
 */
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

/**
 * @brief Free allocated resources
 *
 * @param data Program data structure
 */
void freeResources(program_data_t *data){
    for(int i = 0; i < (* data).argc; i++){
        free((* data).argv[i]);
    }
    free((* data).argv);
}

/**
 * @brief Execute command from shell input
 *
 * @param programData Program data structure
 */
void execute(program_data_t *programData){
    int inputFile, outputFile;
    if (strlen(programData->inputFile) > 0){
        inputFile = open(programData->inputFile, O_RDONLY);
        if(inputFile < 0) {
            perror("open");
            exit(2);
        }

        if(dup2(inputFile, 0) < 0){
            perror("dup2");
            exit(3);
        }
    }

    if (strlen(programData->outputFile) > 0){
        outputFile = open(programData->outputFile, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        if(outputFile < 0) {
            perror("open");
            exit(2);
        }

        if(dup2(outputFile, 1) < 0){
            perror("dup2");
            exit(3);
        }
    }

    execvp((*programData).argv[0], (*programData).argv);
    perror("execvp");

    if (strlen(programData->inputFile) > 0) {
        close(inputFile);
    }

    if (strlen(programData->outputFile) > 0) {
        close(outputFile);
    }

    exit(1);
}

/**
 * @brief Thread what is executing commands in shell.
 *
 * @param arg Global data
 * @return Zero
 */
void *threadRun(void *arg){
    global_data_t *data = (global_data_t *) arg;

    char buffer[BUFFER_SIZE];
    program_data_t programData;

    while(!data->end) {
        bufferWait();

        strcpy(buffer, data->buffer);

        // init programData data structure
        programData.background = false;
        pthread_mutex_lock(&pidMutex);
        data->runningProcessPid = 0;
        pthread_mutex_unlock(&pidMutex);
        memset(programData.inputFile, 0, sizeof(programData.inputFile));
        memset(programData.outputFile, 0, sizeof(programData.outputFile));

        // search for run on background & char
        char *ampersand = strchr(buffer, '&');
        if (ampersand != NULL){
            programData.background = true;
            ampersand[0] = '\0';
        }

        // load input file and clear it from buffer
        char *inputFile = findRedirect(buffer, '<');
        strncpy(programData.inputFile, inputFile, lengthOfArgument(inputFile));
        memset(inputFile, ' ', lengthOfArgument(inputFile));

        // load output file and clear it from buffer
        char *outputFile = findRedirect(buffer, '>');
        strncpy(programData.outputFile, outputFile, lengthOfArgument(outputFile));
        memset(outputFile, ' ', lengthOfArgument(outputFile));

        // load program arguments
        if(parseArgsFromBuffer(&programData, buffer)){
            data->end = true;
            break;
        }

        if(programData.argc > 0) {
            // Exit on exit command
            if(!strcmp(programData.argv[0], "exit")){
                data->end = true;
                bufferSignal();
                break;
            }

            int pid = fork();
            if (pid == -1) {
                perror("fork failure");
                continue;
            } else if (pid > 0) {
                //parent
                if (!programData.background) {
                    pthread_mutex_lock(&pidMutex);
                    data->runningProcessPid = pid;
                    pthread_mutex_unlock(&pidMutex);
                    waitpid(pid, NULL, 0);
                }
            } else {
                //child
                execute(&programData);
            }
        }

        freeResources(&programData);

        bufferSignal();
    }

    freeResources(&programData);
    return 0;
}

/**
 * @brief Thread what is reading input in shell.
 *
 * @param arg Global data
 * @return Zero
 */
void *threadRead(void *arg){
    global_data_t *data = (global_data_t *) arg;

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

/**
 * @brief Signal handler for SICHILD.
 *
 * @param sig Signal number
 */
void sigChildHandler(int sig){
    (void) sig; // avoid warning
    int returnCode;
    pid_t pid = wait(&returnCode);
    if(pid < 0){
        return;
    }
    pthread_mutex_lock(&pidMutex);
    if (pid == global_data.runningProcessPid){ // running process is not child
        pthread_mutex_unlock(&pidMutex);
        return;
    }
    pthread_mutex_unlock(&pidMutex);

    if(WIFEXITED(returnCode)){
        fprintf(stderr, "\nProcess [%d] finished with return code: %d\n", pid, WEXITSTATUS(returnCode));
    }
    else if(WIFSTOPPED(returnCode)) {
        fprintf(stderr, "\nProcess [%d] stopped with signal: %d\n", pid, WSTOPSIG(returnCode));
    }
    else if(WIFSIGNALED(returnCode)){
        fprintf(stderr, "\nProcess [%d] terminated with signal: %d\n", pid, WTERMSIG(returnCode));
    }
    else {
        fprintf(stderr, "\nProcess [%d] is no longer running\n", pid);
    }
    fflush(stderr);
    printf("$ ");
    fflush(stdout);
}

/**
 * @brief Signal handler for SIGINT.
 *
 * @param sig Signal number
 */
void sigIntHandler(int sig){
    pthread_mutex_lock(&pidMutex);
    if (global_data.runningProcessPid > 0){
        kill(global_data.runningProcessPid, sig);
    }
    pthread_mutex_unlock(&pidMutex);
}


int main() {
    pthread_t pthread_read;
    pthread_t pthread_run;

    global_data.end = false;
    global_data.runningProcessPid = 0;

    struct sigaction sigactionSigInt;
    sigactionSigInt.sa_flags = 0;
    sigactionSigInt.sa_handler = sigIntHandler;
    sigemptyset(&sigactionSigInt.sa_mask);
    sigaction(SIGINT, &sigactionSigInt, NULL);

    struct sigaction sigactionSigChild;
    sigactionSigChild.sa_flags = 0;
    sigactionSigChild.sa_handler = sigChildHandler;
    sigemptyset(&sigactionSigChild.sa_mask);
    sigaction(SIGCHLD, &sigactionSigChild, NULL);

    if(pthread_create(&pthread_read, NULL, threadRead, (void *) &global_data)){
        fprintf(stderr, "Error: Error while creation of Read thread.\n");
        return 1;
    }

    if(pthread_create(&pthread_run, NULL, threadRun, (void *) &global_data)){
        fprintf(stderr, "Error: Error while creation of Run thread.\n");
        return 1;
    }

    pthread_join(pthread_read, NULL);
    pthread_join(pthread_run, NULL);

    //Free resources
    pthread_mutex_destroy(&pidMutex);
    pthread_mutex_destroy(&bufferMutex);
    pthread_cond_destroy(&bufferCond);

    return 0;
}