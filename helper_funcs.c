#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* pid_t                   */
#include <unistd.h>    /* execvp(), fork()        */
#include <signal.h>
#include "parse.h"     /* Task struct */
#include <fcntl.h>     /* open()      */
#include "helper_funcs.h"
#include <ctype.h>

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
    void (*old_in)(int);
    void (*old_out)(int);

    old_in = signal(SIGTTIN, SIG_IGN);
    old_out = signal(SIGTTOU, SIG_IGN);

    tcsetpgrp (STDIN_FILENO, pgid);
    tcsetpgrp (STDOUT_FILENO, pgid);

    signal (SIGTTIN, old_in);
    signal (SIGTTOU, old_out);
}

int find_job_index(pid_t child_pid, Job *jobs, int num_jobs)
{
    int i, j;
    int skip = 0;
    int job_index_of_child = 0;

    for (i = 0; i < num_jobs; i++) {

        for (j = 0; j < jobs[i].npids; j++) {
            if (child_pid == jobs[i].pid[j]) {
                job_index_of_child = i;
                skip = 1;
                break;
            }
        }
        if (skip)
            break;
    }

    return job_index_of_child;
}

void print_job_info(int job_num, Job *job, int done)
{
    switch (job->status) {
        case STOPPED:
            fprintf(stderr, "\n[%d]+ Stopped \t%s\n", job_num, job->name);
            break;
        case BG:
            if (done)
                fprintf(stderr, "[%d]+ Done \t%s\n", job_num, job->name);

            else {
                int t;

                fprintf(stderr, "[%d] ", job_num);

                for (t = 0; t < job->npids; t++)
                    fprintf(stderr, "%d ", job->pid[t]);

                fprintf(stderr, "\n");
            }
            break;
        default:
            break;
    }
}

static char *status_to_string(JobStatus status)
{
    char *string = NULL;

    switch (status) {
        case STOPPED: string = "Stopped"; break;
        case TERM:    string = "Done";    break;
        case BG:      string = "Running"; break;
        case FG:      string = "Running"; break;
        default:
            break;
    }
    
    return string;
}

void jobs_cmd()
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
        if (jobs[i].name != NULL)
            fprintf(stdout, "[%d]+ %-10s\t%s\n", 
                    i, status_to_string(jobs[i].status), jobs[i].name);
}

static const char *fg_usage = "Usage: fg %<job number>\n";

void fg_cmd(char **argv)
{
    char *arg_string = NULL;
    int target_job = 0;
    int string_size = 0;

    arg_string = *(argv + 1);

    if (arg_string == NULL) {
        fprintf(stdout, "%s", fg_usage);
        return ;
    }

    string_size = strlen(arg_string);

    if (arg_string[0] != '%' || string_size < 2) {
        fprintf(stdout, "%s", fg_usage);
        return ;
    }

    arg_string = &argv[1][1];

    int i;
    for (i = 0; arg_string[i]; i++)
        if (!isdigit(arg_string[i])) {
            fprintf(stdout, "%s", fg_usage);
            return ;
        }

    target_job = atoi(arg_string);

    if (jobs[target_job].name == NULL) {
        fprintf(stderr, "fg :%d: no such job\n", target_job);
        return ;
    }

    fprintf(stdout, "%s\n", jobs[target_job].name);

    jobs[target_job].status = FG;

    kill(-jobs[target_job].pgid, SIGCONT);
    set_fg_pgid(jobs[target_job].pgid);
}
