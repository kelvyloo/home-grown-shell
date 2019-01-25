#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* pid_t                   */
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()*/
#include <unistd.h>    /* execvp(), fork()        */

#include "parse.h"     /* Task struct */
#include <fcntl.h>     /* open()      */

static int redirect_output(char *outfile) 
{
    unsigned int out_fd = 0;

    if (outfile != NULL) {
        out_fd = open(outfile, O_CREAT | O_RDONLY | O_WRONLY, 0644);
        
        if (out_fd == -1) {
            printf("pssh: failed to open/create file %s\n", outfile);
            return EXIT_FAILURE;
        }
        else {
            if (dup2(out_fd, STDOUT_FILENO) == -1) {
                printf("pssh: failed to redirect output to %s\n", outfile);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

static int redirect_input(char *infile) 
{
    unsigned int in_fd = 0;

    if (infile != NULL) {
        in_fd = open(infile, O_RDONLY);
        
        if (in_fd == -1) {
            printf("pssh: failed to open/create file %s\n", infile);
            return EXIT_FAILURE;
        }
        else {
            if (dup2(in_fd, STDIN_FILENO) == -1) {
                printf("pssh: failed to redirect input to %s\n", infile);
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

int execute_cmd(Parse *P, int t)
{
    pid_t pid;

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "error -- failed to fork()");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        /* only executed by the PARENT process */
        int child_ret;
        waitpid(pid, &child_ret, 0);
    } 
    
    else {
        if (redirect_output(P->outfile)) {
            printf("Failed to redirect output\n");
            exit(EXIT_FAILURE);
        }

        if (redirect_input(P->infile)) {
            printf("Failed to redirect input\n");
            exit(EXIT_FAILURE);
        }

        if(execvp(P->tasks[t].cmd, P->tasks[t].argv)) {
            printf("Failed to exec %s\n", P->tasks[t].cmd);
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
