#ifndef _helper_funcs__h_
#define _helper_funcs__h_

#include "parse.h"

/**
 * Checks if an output file is specified and redirects stdout to that file
 *
 * @param outfile name of output file
 *
 * @return 0 success, 1 failure
 */
int check_and_redirect_output(char *outfile);


/**
 * Checks if an input file is specified and redirects stdout to that file
 *
 * @param infile name of input file
 *
 * @return 0 success, 1 failure
 */
int check_and_redirect_input(char *infile);

typedef enum {
    STOPPED,
    TERM,
    BG,
    FG,
} JobStatus;

typedef struct {
    char *name;
    pid_t *pid;
    unsigned int npids;
    pid_t pgid;
    JobStatus status;
} Job;

void set_job_name(Job *job, Task task);

void set_job_status(Job *job, JobStatus status);

void set_job_npids(Job *job, unsigned int npids);

void set_job_pgid(Job *job, pid_t pgid);

void set_job_pid(Job *job, unsigned int t, pid_t pid);

void set_pgid(pid_t pid, pid_t pgid);

void set_fg_pgid(pid_t pgid);

/**
 * Forks parent process, open pipes for possible redirection, and executes cmd
 *
 * @param P the Parse struct which holds the task(s)
 * @param t index for which task to execute
 *
 * @return 0 success, 1 failure
 */
int execute_cmd(Parse *P, unsigned int t, Job *job, pid_t *pid);

#endif
