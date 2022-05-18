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

char hostname[STD_BUFF_SIZE] = {0};
char currentDir[STD_BUFF_SIZE] = {0};
char oldDir[STD_BUFF_SIZE] = {0};
char oldDirVolatile[STD_BUFF_SIZE] = {0};

pid_t childPid;

struct stat buff;

int main(int argc __attribute__((unused)), char **argv __attribute__((unused))) {
    char *toks[STD_PARAMS_SIZE] = {0};
    char shellInput[STD_INPUT_SIZE] = {0};
    char oldWd[STD_INPUT_SIZE] = {0};
    char *oldWdVolatile;
    char *username;
    username = getenv("USER");
    gethostname(hostname, sizeof(hostname));
    char *unused = getcwd(currentDir, sizeof(currentDir));
    (void) unused;

    while (1) {
        int counter = 0;
        int stopNeeded = -1;
        printDir(username, hostname, currentDir);
        unused = fgets(shellInput, sizeof(shellInput), stdin);

        //erase the last character if it is '\n'
        char *position = NULL;
        if ((position = strchr(shellInput, '\n')) != NULL) {
            *position = '\0';
        }

        char *token = strtok(shellInput, " ");

        while (token != NULL) {
            toks[counter] = strdup(token);
            token = strtok(NULL, " ");
            counter++;
        }

        toks[counter] = NULL;

        int pipeFound = 0, pipeIndex = 0;
        for (int i = 0; i < counter; ++i) {
            if (strcmp(toks[i], "|") == 0) {
                pipeFound = 1;
                pipeIndex = i;
                break;
            }
        }
        //Handle cd built-in
        if (toks[0] == NULL)
            continue;
        int found = oldCwdFound(toks);
        if (strcmp(toks[0], "cd") == 0) {
            if ((toks[1] == NULL) || (toks[1][0] == '~')) {
                oldWdVolatile = getcwd(oldDirVolatile, sizeof(oldDir));
                if (chdir("/home") == 0) {
                    getcwd(currentDir, sizeof(currentDir));
                    strcpy(oldWd, oldWdVolatile);
                } else {
                    printf("%s\n", strerror(errno));
                }

            } else if (found) {
                oldWdVolatile = getcwd(oldDirVolatile, sizeof(oldDir));
                if (chdir(oldWd) == 0) {
                    getcwd(currentDir, sizeof(currentDir));
                    strcpy(oldWd, oldWdVolatile);
                    getcwd(currentDir, sizeof(currentDir));
                } else {
                    printf("%s: %s\n", toks[1], strerror(errno));
                }

            } else {
                oldWdVolatile = getcwd(oldDirVolatile, sizeof(oldDir));
                if (chdir(toks[1]) == 0) {
                    getcwd(currentDir, sizeof(currentDir));
                    strcpy(oldWd, oldWdVolatile);
                } else {
                    printf("%s: %s\n", toks[1], strerror(errno));
                }
            }
        } else {
            if (!pipeFound) {

                int background = checkBg(toks, counter);

                //Handle commands
                pid_t pid = fork();

                if (pid < 0) {
                    printf("%s \n", strerror(errno));
                    exit(1);
                }
                stopNeeded = exitStatus(toks);

                if (pid == 0 && stopNeeded == -1) {

                    pathFinder(toks, shellInput, currentDir);
                    inOutReDirector(toks);

                    if (execv(shellInput, toks) != 0) {
                        char *errMsg = strerror(errno);
                        printf("%s \n", errMsg);
                        //clean the old contents of toks
                        for (int i = 0; i < STD_PARAMS_SIZE; ++i) {
                            free(toks[i]);
                            toks[i] = NULL;
                        }
                        exit(1);
                    }
                }

                if (pid > 0) {
                    if(background == 0){
                        int status;
                        childPid = waitpid(pid, &status, 0);
                        (void) childPid;
                    }
                    else{
                        int status;
                        childPid = waitpid(pid, &status, WNOHANG);
                        (void) childPid;
                    }
                }

            }
                //Handle pipes
            else {
                char *args1[STD_PARAMS_SIZE] = {0}, *args2[STD_PARAMS_SIZE] = {0};

                for (int i = 0; i < pipeIndex; ++i) {
                    args1[i] = toks[i];
                }
                int i = 0;
                for (int j = pipeIndex + 1; j < counter; ++j) {
                    args2[i] = toks[j];
                    ++i;
                }
                if (strcmp(args1[0], "exit") == 0 || strcmp(args2[0], "exit") == 0) {
                    stopNeeded = 0;
                } else {
                    char args1Buff[STD_INPUT_SIZE] = {0}, args2Buff[STD_INPUT_SIZE] = {0};
                    pathFinder(args1, args1Buff, currentDir);
                    pathFinder(args2, args2Buff, currentDir);

                    pipeExecuter(args1, args2, args1Buff, args2Buff);
                }
            }

        }
        //Kill zombie if there is one
        signal(SIGCHLD, handler);

        //clean the old contents of toks
        for (int i = 0; i < STD_PARAMS_SIZE; ++i) {
            free(toks[i]);
            toks[i] = NULL;
        }
        if (stopNeeded != -1) {
            exit(stopNeeded);
        }
    }
    return 0;
}