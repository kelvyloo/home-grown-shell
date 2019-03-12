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
#include <errno.h>

static void init_job(Job *job)
{
    job->pid = NULL;
    job->name = NULL;
    job->npids = 0;
    job->pgid = 0;
    job->status = 0;
}

void init_jobs()
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
        init_job(&jobs[i]);
}

/* TODO fix */
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

void remove_bg_jobs()
{
    int i;

    for (i = 0; i < MAX_JOBS; i++)
        if (jobs[i].status == TERM) {
            print_job_info(i, &jobs[i]);
            destroy_job(&jobs[i]);
        }
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

int assign_lowest_job_num()
{
    int i;
    int lowest_available_job_num = 0;

    for (i = 0; i < MAX_JOBS; i++)
        if (jobs[i].name == NULL) {
            lowest_available_job_num = i;
            break;
        }

    return lowest_available_job_num;
}

int find_job_index(pid_t child_pid)
{
    int i, j;
    int skip = 0;
    int job_index_of_child = 0;

    for (i = 0; i < MAX_JOBS; i++) {

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

void print_bg_job(int job_num, Job *job)
{
    int t;

    fprintf(stdout, "[%d] ", job_num);

    for (t = 0; t < job->npids; t++)
        fprintf(stdout, "%d ", job->pid[t]);

    fprintf(stdout, "\n");
}

void print_job_info(int job_num, Job *job)
{

    switch (job->status) {
        case STOPPED:
            fprintf(stdout, "[%d]+ Suspended \t%s\n", job_num, job->name);
            break;
        case TERM:
            fprintf(stdout, "[%d]+ Done \t%s\n", job_num, job->name);
            break;
        case BG:
            fprintf(stdout, "[%d]+ Continued \t%s\n", job_num, jobs->name);
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

static int invalid_input(char *arg_string)
{
    int i;

    for (i = 0; arg_string[i]; i++)
        if (!isdigit(arg_string[i]))
            return 1;

    return 0;
}

static const char *fg_usage = "Usage: fg %<job number>\n";
static const char *bg_usage = "Usage: bg %<job number>\n";

void sigcont_cmd(char **argv, int fg)
{
    char *arg_string = NULL;
    int target_job = 0;
    int string_size = 0;

    arg_string = *(argv + 1);

    if (arg_string == NULL) {
        fprintf(stdout, "%s", (fg) ? fg_usage : bg_usage);
        return ;
    }

    string_size = strlen(arg_string);

    if (arg_string[0] != '%' || string_size < 2) {
        fprintf(stdout, "%s", (fg) ? fg_usage : bg_usage);
        return ;
    }

    arg_string = &argv[1][1];

    if (invalid_input(arg_string)) {
        fprintf(stdout, "pssh: invalid job number: [%s]\n", arg_string);
        return ;
    }

    target_job = atoi(arg_string);

    if (jobs[target_job].name == NULL) {
        fprintf(stdout, "pssh: invalid job number: [%d]\n", target_job);
        return ;
    }

    fprintf(stdout, "%s\n", jobs[target_job].name);

    jobs[target_job].status = (fg) ? FG : STOPPED;

    kill(-jobs[target_job].pgid, SIGCONT);

    if (fg)
        set_fg_pgid(jobs[target_job].pgid);
}

static int send_signal(pid_t pid, int signal) 
{
    int ret;

    ret = kill(-pid, signal);

    if (ret == -1 || signal == 0) {
        switch (errno) {
            case EINVAL:
                printf("Invalid signal specified\n");
                break;
            case EPERM:
                printf("Do not have permission to send PID %d signals\n", pid);
                break;
            case ESRCH:
                printf("PID %d does not exist\n", pid);
                break;
            default:
                printf("PID %d exists and can receive signals\n", pid);
                break;
        }
    }

    return 0;
}

static const char *kill_usage = "Usage: kill [-s <signal>] <pid> | %<job> ...\n";

void kill_cmd(char **argv)
{
    size_t argc = 0;
    int i, signal, job;
    pid_t pid;
    char *arg_string = NULL;

    for (i = 0; argv[i]; i++)
        argc++;

    if (argc < 2) {
        fprintf(stdout, "%s", kill_usage);
        return ;
    }

    if (!strcmp (argv[1], "-s")) {
        if (invalid_input(argv[2])) {
            fprintf(stdout, "pssh: invalid signal [%s]\n", argv[2]);
            return ;
        }
        signal = atoi(argv[2]);
        i = 3;
    }
    else {
        signal = SIGTERM;
        i = 1;
    }

    for (; argv[i]; i++) {
        if (argv[i][0] == '%') {
            job = 1;
            arg_string = &argv[i][1];
        }
        else {
            job = 0;
            arg_string = argv[i];
        }
            
        if (invalid_input(arg_string)) {
            fprintf(stdout, "pssh: invalid job number: [%s]\n", argv[i]);
            return ;
        }

        pid = atoi(arg_string);
        
        pid = (job) ? jobs[pid].pgid : pid;

        send_signal(pid, signal);
    }

    return ;
}
