#include "header.hpp"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "header.hpp"
#define STD_BUFF_SIZE 1024
#define STD_INPUT_SIZE 256
#define STD_PARAMS_SIZE 32


pid_t childPid;

struct stat buff;
const void *printDir(char *userName, char *hostName, char *currentDirectory) {
    printf("%s@%s:%s $ ", userName, hostName, currentDirectory);
    return NULL;
}

int oldCwdFound(char **toks) {
    if (toks[1] != NULL && strcmp(toks[1], "$OLDPWD") == 0) {
        return 1;
    }
    else
        return 0;
}

void pathFinder(char **toks, char *input, char *cwd) {
    int counter = 0;
    char *path, *pathName, *pathNameCopy, directoryPath[STD_INPUT_SIZE / 2][STD_INPUT_SIZE / 2] = {0};

    pathName = getenv("PATH");
    pathNameCopy = strdup(pathName);

    //Parse $PATH directories into tokens
    path = strtok(pathNameCopy, ":");
    while (path != NULL) {
        strcat(directoryPath[counter], path);
//        printf("%d.[%s], ", counter, directoryPath[counter]); // show parsed dirs into tokens
        path = strtok(NULL, ":");
        counter++;
    }
    free(pathNameCopy);
    pathNameCopy = NULL;

    //Find the $PATH directoryPath that executable resides in
    for (int j = 0; j <= counter; ++j) {
        chdir(directoryPath[j]);
        if (stat(toks[0], &buff) == 0) {
            strcat(directoryPath[j], "/");
            strcat(directoryPath[j], toks[0]);
            strcpy(input, directoryPath[j]);
//            printf("\n|%s|\n", input); // show matched path
            chdir(cwd);
            break;
        }
    }
}

void inOutReDirector(char **toks) {
    fflush(0);
    int fd0, fd1, fd2, fd3, in = 0, out = 0, err = 0, outAppend = 0;
    char inputFile[64] = {0}, outputFile[64] = {0}, outputAppendFile[64] = {0}, errorFile[64] = {0};
    for (int i = 0; toks[i] != NULL; i++) {
        if (strcmp(toks[i], "<") == 0) {
            toks[i] = NULL;
            strcpy(inputFile, toks[i + 1]);
            toks[i + 1] = NULL;
            in = 1;
        } else if ((strcmp(toks[i], ">") == 0) || (strcmp(toks[i], "1>") == 0)) {
            toks[i] = NULL;
            strcpy(outputFile, toks[i + 1]);
            toks[i + 1] = NULL;
            out = 1;
        } else if (strcmp(toks[i], "2>") == 0) {
            toks[i] = NULL;
            strcpy(errorFile, toks[i + 1]);
            toks[i + 1] = NULL;
            err = 1;
        } else if ((toks[i][0] == '>') && (toks[i][1] == '>')) {
            toks[i] = NULL;
            strcpy(outputAppendFile, toks[i + 1]);
            toks[i + 1] = NULL;
            outAppend = 1;
        }

    }

    //input redirection
    if (in == 1) {

        if ((fd0 = open(inputFile, O_RDONLY, 0)) < 0) {
            printf("Couldn't open input file: %s\n", strerror(errno));
            exit(1);
        }
        dup2(fd0, STDIN_FILENO);
        close(fd0);
    }
    //output redirection
    if (out == 1) {

        if ((fd1 = open(outputFile, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0) {
            printf("Couldn't open output file: %s", strerror(errno));
            exit(1);
        }
        dup2(fd1, STDOUT_FILENO);
        close(fd1);
    }
    //error redirection
    if (err == 1) {

        if ((fd2 = open(errorFile, O_CREAT | O_WRONLY | O_TRUNC, 0644)) < 0) {
            printf("Couldn't open output file: %s\n", strerror(errno));
            exit(1);
        }
        dup2(fd2, STDERR_FILENO);
        close(fd2);
    }
    //output Append redirection
    if (outAppend == 1) {
        if ((fd3 = open(outputAppendFile, O_CREAT | O_WRONLY | O_APPEND, 0644)) < 0) {
            printf("Couldn't open output file: %s", strerror(errno));
            exit(1);
        }
        dup2(fd3, STDOUT_FILENO);
        close(fd3);
    }
}

void pipeExecuter(char **args1, char **args2, char *args1Buff, char *args2Buff) {
    int fd[2];
    pid_t pidChild1, pidChild2;
    pipe(fd);
    pidChild1 = fork();

    if (pidChild1 < 0) {
        perror("Error creating fork\n");
        exit(1);
    }

    if (pidChild1 == 0) {
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        execv(args1Buff, args1);
        printf("Error on \"Writing\" side of the pipe: %s\n", strerror(errno));
        exit(1);
    } else {
        waitpid(pidChild1, NULL, 0);
        pidChild2 = fork();
        if (pidChild2 < 0) {
            perror("Error creating fork");
            exit(1);
        }
        if (pidChild2 == 0) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            execv(args2Buff, args2);
            printf("Error on \"Reading\" side of the pipe: %s\n", strerror(errno));
            exit(1);
        } else {
            close(fd[0]);
            close(fd[1]);
            waitpid(pidChild2, NULL, 0);
        }
    }
}

int exitStatus(char **toks) {
    if (strcmp(toks[0], "exit") == 0) {
        if (toks[1] == NULL) {
            return 0;
        } else
            return atoi(toks[1]);
    }
    return -1;
}

int checkBg(char **toks, int counter){
    if(toks[0] != NULL && strcmp(toks[counter - 1], "&") == 0){
        free(toks[counter - 1]);
        toks[counter - 1] = NULL;
        return 1;
    }
    else
        return 0;
}

void handler(int num){
    waitpid(childPid, NULL, WNOHANG);
}