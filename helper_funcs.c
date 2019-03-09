#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* pid_t                   */
#include <unistd.h>    /* execvp(), fork()        */
#include <signal.h>
#include "parse.h"     /* Task struct */
#include <fcntl.h>     /* open()      */
#include "helper_funcs.h"

static void init_job(Job *job)
{
    job->pid = NULL;
    job->name = NULL;
    job->npids = 0;
    job->pgid = 0;
    job->status = 0;
}

void init_jobs(Job *jobs, int num_jobs)
{
    int i;

    for (i = 0; i < num_jobs; i++)
        init_job(&jobs[i]);
}

int assign_lowest_job_num(Job *jobs, int num_jobs)
{
    int i;
    int lowest_available_job_num = 0;

    for (i = 0; i < num_jobs; i++)
        if (jobs[i].name == NULL) {
            lowest_available_job_num = i;
            break;
        }

    return lowest_available_job_num;
}

static void build_job_name(Parse *P, char *job_name)
{
    int i, j;

    for (i = 0; i < P->ntasks; i++) {

        for (j = 0; P->tasks[i].argv[j]; j++) {
            if (i == 0 && j == 0)
                strncpy(job_name, P->tasks[i].argv[j], 
                        sizeof(job_name)/sizeof(char));
            else
                strncat(job_name, P->tasks[i].argv[j], 
                        sizeof(job_name)/sizeof(char));

            strncat(job_name, " ", sizeof(job_name)/sizeof(char));
        }
        
        if (i != P->ntasks-1)
            strncat(job_name, "| ", sizeof(job_name)/sizeof(char));
    }
}

void create_job(Job *job, Parse *P, pid_t pgid)
{
    job->pid = malloc(P->ntasks * sizeof(pid_t));
    job->name = malloc(256);

    build_job_name(P, job->name);

    //strcpy(job->name, P->tasks[0].cmd);
    job->npids = P->ntasks;
    job->pgid = pgid;
    job->pid[0] = job->pgid;
    job->status = (P->background) ? BG : FG;
}

void destroy_job(Job *job)
{
    free(job->pid);
    free(job->name);

    job->pid = NULL;
    job->name = NULL;
    job->npids = 0;
    job->pgid = 0;
    job->status = 0;
}

void set_fg_pgid(pid_t pgid)
{
    void (*old)(int);

    old = signal(SIGTTOU, SIG_IGN);

    tcsetpgrp (STDIN_FILENO, pgid);
    tcsetpgrp (STDOUT_FILENO, pgid);

    signal (SIGTTOU, old);
}

int is_job_stopped(pid_t child_pid, Job *jobs, int num_jobs)
{
    int i, j;
    int skip = 0;
    int stopped_job;

    for (i = 0; i < num_jobs; i++) {

        for (j = 0; j < jobs[i].npids; j++) {
            if (child_pid == jobs[i].pid[j]) {
                stopped_job = i;
                skip = 1;
                break;
            }
        }
        if (skip)
            break;
    }
    
    return stopped_job;
}

int is_job_done(pid_t child_pid, Job *jobs, int num_jobs, int *killed)
{
    int i, j;
    int skip = 0;
    int finished_job = 0;

    for (i = 0; i < num_jobs; i++) {

        for (j = 0; j < jobs[i].npids; j++) {
            if (child_pid == jobs[i].pid[j]) {
                killed[i]++;
                finished_job = (killed[i] == jobs[i].npids) ? i+1 : 0;
                skip = 1;
                break;
            }
        }
        if (skip)
            break;
    }

    return finished_job;
}

void print_job_info(int job_num, Job *job, int done)
{
    switch (job->status) {
        case STOPPED:
            printf("\n[%d]+ Stopped \t%s\n", job_num+1, job->name);
            break;
        case BG:
            if (done)
                printf("[%d]+ Done \t%s\n", job_num+1, job->name);

            else {
                int t;

                printf("[%d] ", job_num+1);

                for (t = 0; t < job->npids; t++)
                    printf("%d ", job->pid[t]);

                printf("\n");
            }
            break;
        default:
            break;
    }
}
