#ifndef _helper_funcs__h_
#define _helper_funcs__h_

#include "parse.h"

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

void create_job(Job *job, Parse *P, pid_t pgid);

void set_fg_pgid(pid_t pgid);

void print_background_job(int job_num, Job *job, int done);

#endif
