#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* pid_t                   */
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()*/
#include <unistd.h>    /* execvp(), fork()        */

#include "parse.h"     /* Task struct */
#include <fcntl.h>     /* open()      */
#include "helper_funcs.h"
#include <string.h>

void create_job(Job *job, Parse *P, pid_t pgid)
{
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
