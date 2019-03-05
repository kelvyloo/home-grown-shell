#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* pid_t                   */
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()*/
#include <unistd.h>    /* execvp(), fork()        */

#include "parse.h"     /* Task struct */
#include <fcntl.h>     /* open()      */

#define READ_SIDE 0
#define WRITE_SIDE 1

int check_and_redirect_output(char *outfile) 
{
    unsigned int out_fd = 0;

    if (outfile != NULL) {
        out_fd = open(outfile, O_CREAT | O_WRONLY, 0644);
        
        if (out_fd == -1) {
            printf("pssh: failed to open/create file %s\n", outfile);
            return EXIT_FAILURE;
        }

        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            printf("pssh: failed to redirect output to %s\n", outfile);
            return EXIT_FAILURE;
        }
        close(out_fd);
    }
    return EXIT_SUCCESS;
}

int check_and_redirect_input(char *infile) 
{
    unsigned int in_fd = 0;

    if (infile != NULL) {
        in_fd = open(infile, O_RDONLY);
        
        if (in_fd == -1) {
            printf("pssh: failed to open file %s\n", infile);
            return EXIT_FAILURE;
        }

        if (dup2(in_fd, STDIN_FILENO) == -1) {
            printf("pssh: failed to redirect input to %s\n", infile);
            return EXIT_FAILURE;
        }
        close(in_fd);
    }
    return EXIT_SUCCESS;
}

int set_pgid(pid_t pid, pid_t pgid)
{
    if (setpgid(pid, pgid))
        fprintf(stderr, "error -- setpgid() failed\n");

    return 0;
}

int set_fg_pgid(pid_t pgid)
{
    void (*old)(int);

    old = signal(SIGTTOU, SIG_IGN);

    tcsetpgrp (STDIN_FILENO, pgid);
    tcsetpgrp (STDOUT_FILENO, pgid);

    signal (SIGTTOU, old);

    return 0;
}

int execute_cmd(Parse *P, unsigned int t)
{
    int pipe_fd[2] = {0};
    static int input_fd = 0;
    pid_t *pid = malloc(P->ntasks * sizeof(pid_t));

    if (pipe(pipe_fd) == -1) {
        fprintf(stderr, "error -- failed to create pipe");
        return EXIT_FAILURE;
    }

    pid[t] = fork();
    set_pgid(pid[t], pid[0]);

    if (pid[t] < 0) {
        fprintf(stderr, "error -- failed to fork()");
        return EXIT_FAILURE;
    }

    else if (pid[t] > 0) {
        if (!P->background) {
            set_fg_pgid(pid[t]);
        }
        else {
            set_fg_pgid(getpid());
            printf("[%d] %d\n", t+1, pid[t]);
        }

        /* Parent process */
        wait(NULL);
        close(pipe_fd[WRITE_SIDE]);
         // Hold previous read fd for next task in multipipe
        input_fd = pipe_fd[READ_SIDE];
    }
    else {
        /* Child process */
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            fprintf(stderr, "error -- dup2() failed for input_fd -> STDIN\n");
            return EXIT_FAILURE;
        }
        if (t == 0) {
            if (check_and_redirect_input(P->infile)) { return EXIT_FAILURE; }
        }

        if (t != (P->ntasks-1)) { 
            /* If task is not the last one -> write to pipe not stdout */
            if (dup2(pipe_fd[WRITE_SIDE], STDOUT_FILENO) == -1) {
                fprintf(stderr, "error -- dup2() failed for WRITE_SIDE -> STDOUT\n");
                return EXIT_FAILURE;
            }
        }
        close(pipe_fd[READ_SIDE]);

        if (t == (P->ntasks-1)) {
            if (check_and_redirect_output(P->outfile)) { return EXIT_FAILURE; }
        }

        if (execvp(P->tasks[t].cmd, P->tasks[t].argv)) {
            printf("Failed to exec %s\n", P->tasks[t].cmd);
        }
    }

    return EXIT_SUCCESS;
}
