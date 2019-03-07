#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* pid_t                   */
#include <unistd.h>    /* execvp(), fork()        */
#include <signal.h>
#include "parse.h"     /* Task struct */
#include <fcntl.h>     /* open()      */
#include "helper_funcs.h"

void create_job(Job *job, Parse *P, pid_t pgid)
{
    job->pid = malloc(P->ntasks * sizeof(pid_t));

    job->name = P->tasks[0].cmd;
    job->npids = P->ntasks;
    job->pgid = pgid;
    job->pid[0] = job->pgid;
    job->status = (P->background) ? BG : FG;
}

void set_fg_pgid(pid_t pgid)
{
    void (*old)(int);

    old = signal(SIGTTOU, SIG_IGN);

    tcsetpgrp (STDIN_FILENO, pgid);
    tcsetpgrp (STDOUT_FILENO, pgid);

    signal (SIGTTOU, old);
}

void print_background_job(int job_num, Job *job, int done)
{
    if (done) {
        printf("\n[%d]+ Done \t%s\n", job_num, job->name);
    }
    else {
        int t;

        printf("[%d] ", job_num);

        for (t = 0; t < job->npids; t++)
            printf("%d ", job->pid[t]);

        printf("\n");
    }
}
