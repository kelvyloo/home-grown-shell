#ifndef _helper_funcs__h_
#define _helper_funcs__h_

#include "parse.h"

#define MAX_JOBS 100
#define READ_SIDE 0
#define WRITE_SIDE 1

typedef enum {
    STOPPED,
    TERM,
    BG,
    FG,
} JobStatus;

typedef struct {
    char *name;
    pid_t *pid;
    int npids;
    pid_t pgid;
    JobStatus status;
} Job;

Job jobs[MAX_JOBS];

void init_jobs(Job *jobs, int num_jobs);

int assign_lowest_job_num(Job *jobs, int num_jobs);

void create_job(Job *job, Parse *P, pid_t pgid);

void destroy_job(Job *job);

void set_fg_pgid(pid_t pgid);

int find_job_index(pid_t child_pid, Job *jobs, int num_jobs);

void print_job_info(int job_num, Job *job, int done);

#endif
