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

/* Initialize array of Job structs
 *
 * @param none
 *
 * @return none
 * */
void init_jobs();

/* Initialize a new job
 *
 * @param job   ptr to array loc of new job struct
 * @param P     Parse struct with job info
 * @param pgid  process group id of job
 *
 * @return none
 * */
void create_job(Job *job, Parse *P, pid_t pgid);

/* Remove and free a job
 *
 * @param job  ptr to array loc of job to remove
 *
 * @return none
 * */
void destroy_job(Job *job);

void remove_bg_jobs();

/* Give target process group foreground
 *
 * @param pgid  process group id
 *
 * @return none
 * */
void set_fg_pgid(pid_t pgid);

/* Find the lowest available job number
 *
 * @param none  
 *
 * @return lowest_available_job_num  index of first unassigned job struct
 * */
int assign_lowest_job_num();

/* Find the job associated with the PID of a child
 *
 * @param child_pid  pid of the job's child
 *
 * @return job_index_of_child  job index associated with child
 * */
int find_job_index(pid_t child_pid);

void print_bg_job(int job_num, Job *job);

/* Print status of a job
 *
 * @param job_num  job number
 * @param job      Job struct whose info to print
 * @param done     flag if job done
 *
 * @return none
 * */
void print_job_info(int job_num, Job *job);

void print_job_status_updates(int *signal, int jobs[]);

/* Built-in job command for pssh
 *
 * @param none
 *
 * @return none
 * */
void jobs_cmd();

/* Built-in bg and fg command
 *
 * @param argv  array of strings provided by user
 * @param fg    flag if command is the fg or bg
 *
 * @return none
 * */
void sigcont_cmd(char **argv, int fg);

/* Built-in kill command
 *
 * @param argv  array of strings provided by user
 *
 * @return none
 * */
void kill_cmd(char **argv);

#endif
