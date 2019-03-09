#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>

#include <sys/wait.h>
#include <fcntl.h>
#include <sys/types.h>

#include "builtin.h"
#include "parse.h"

#include "helper_funcs.h"

#define MAX_JOBS 100
#define READ_SIDE 0
#define WRITE_SIDE 1

Job jobs[MAX_JOBS];
int finished_job_num = 0;
int bg_job_finished = 0;

/*******************************************
 * Set to 1 to view the command line parse *
 *******************************************/
#define DEBUG_PARSE 0

void print_banner ()
{
    printf ("                    ________   \n");
    printf ("   ______________________  /_  \n");
    printf ("___  __ \\_  ___/_  ___/_  __ \\ \n");
    printf ("__  /_/ /(__  )_(__  )_  / / / \n");
    printf ("_  .___//____/ /____/ /_/ /_/  \n");
    printf ("/_/ Type 'exit' or ctrl+c to quit\n\n");
}


/* returns a string for building the prompt
 *
 * Note:
 *   If you modify this function to return a string on the heap,
 *   be sure to free() it later when appropirate!  */
static char* build_prompt ()
{
    char *cwd = malloc(PATH_MAX);

    if (getcwd(cwd, PATH_MAX) != NULL) {
        sprintf(cwd, "%s$ ", cwd);
    }
    free(cwd);

    return cwd;
}


/* return true if command is found, either:
 *   - a valid fully qualified path was supplied to an existing file
 *   - the executable file was found in the system's PATH
 * false is returned otherwise */
static int command_found (const char* cmd)
{
    char* dir;
    char* tmp;
    char* PATH;
    char* state;
    char probe[PATH_MAX];

    int ret = 0;

    if (access (cmd, X_OK) == 0)
        return 1;

    PATH = strdup (getenv("PATH"));

    for (tmp=PATH; ; tmp=NULL) {
        dir = strtok_r (tmp, ":", &state);
        if (!dir)
            break;

        strncpy (probe, dir, PATH_MAX);
        strncat (probe, "/", PATH_MAX);
        strncat (probe, cmd, PATH_MAX);

        if (access (probe, X_OK) == 0) {
            ret = 1;
            break;
        }
    }

    free (PATH);
    return ret;
}

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the jobs done! */
void execute_tasks (Parse* P)
{
    int t = 0;
    int input_fd, output_fd;
    int pipe_fd[2];
    pid_t *pid = malloc(P->ntasks * sizeof(pid_t));
    int new_job_num;

    int og_stdin = dup(STDIN_FILENO);
    int og_stdout = dup(STDOUT_FILENO);

    input_fd = og_stdin;
    output_fd = og_stdout;

    new_job_num = assign_lowest_job_num(jobs, MAX_JOBS);

    if (P->infile) {
        input_fd = open(P->infile, O_RDONLY, 0644);
        dup2(input_fd, STDIN_FILENO);
    }

    for (t = 0; t < P->ntasks; t++) {
        pipe(pipe_fd);

        if (t < (P->ntasks-1))
            output_fd = pipe_fd[WRITE_SIDE];
        else
            if (P->outfile)
                output_fd = open(P->outfile, O_CREAT | O_WRONLY, 0644);

        if (is_builtin(P->tasks[t].cmd)) {
            dup2(output_fd, STDOUT_FILENO);
            builtin_execute(P->tasks[t]);
        }
        else if (command_found(P->tasks[t].cmd)) {
            pid[t] = fork();
            setpgid(pid[t], pid[0]);

            /* Parent process*/
            if (pid[t] > 0) {
                input_fd = pipe_fd[READ_SIDE];
                close(pipe_fd[WRITE_SIDE]);

                if (t == 0)
                    create_job(&jobs[new_job_num], P, pid[0]);

                jobs[new_job_num].pid[t] = pid[t];
                set_fg_pgid((!P->background) ? jobs[new_job_num].pgid : getpgrp());
            }
            /* Child process */
            else if (pid[t] == 0) {
                dup2(input_fd, STDIN_FILENO);

                if (t != P->ntasks-1 || P->outfile)
                    dup2(output_fd, STDOUT_FILENO);

                execvp(P->tasks[t].cmd, P->tasks[t].argv);
            }
        }
        else
            fprintf(stderr, "pssh: %s command not found\n", P->tasks[t].cmd);
    }

    if (P->background)
        print_job_info(new_job_num, &jobs[new_job_num], 0);

#if 0
    /* DEBUGGING SHIT */
    printf("---------------------------------\n");
    printf("jobs NAME: %s | PGID: %d | Num proc: %d | Status %d\n", 
            jobs[new_job_num].name, jobs[new_job_num].pgid, jobs[new_job_num].npids, jobs[new_job_num].status);

    for (t = 0; t < P->ntasks; t++)
        printf("pid %d\n", jobs[new_job_num].pid[t]);

    printf("---------------------------------\n");
#endif

    free(pid);

    /* Restore stdin & out */
    dup2(og_stdin, STDIN_FILENO);
    dup2(og_stdout, STDOUT_FILENO);

    close(og_stdin);
    close(og_stdout);
}

void handler_sigttou (int sig)
{
    while (tcgetpgrp(STDOUT_FILENO) != getpid ())
        pause ();
}

void handler_sigttin (int sig)
{
    while (tcgetpgrp(STDIN_FILENO) != getpid ())
        pause ();
}

void handler(int sig)
{
    pid_t child_pid;
    int status;
    int finished_job;
    static int killed[MAX_JOBS] = {0};

    child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);

    if (WIFSTOPPED(status)) {
    }
    else if (WIFCONTINUED(status)) {
    }
    else {
        finished_job = is_job_done(child_pid, jobs, MAX_JOBS, killed);

        if (finished_job) {
            set_fg_pgid(getpgrp());
            killed[finished_job-1] = 0;

            if (jobs[finished_job-1].status == FG)
                destroy_job(&jobs[finished_job-1]);
            else {
                bg_job_finished = 1;
                finished_job_num = finished_job - 1;
            }
        }
    }

    return ;
}

int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;

    print_banner ();

    signal(SIGCHLD, handler);
    signal(SIGTTOU, handler_sigttou);
    signal(SIGTTIN, handler_sigttin);

    init_jobs(jobs, MAX_JOBS);

    while (1) {
        if (bg_job_finished) {
            print_job_info(finished_job_num, &jobs[finished_job_num], bg_job_finished);
            destroy_job(&jobs[finished_job_num]);
            bg_job_finished = 0;
            finished_job_num = 0;
        }

        cmdline = readline (build_prompt());
        if (!cmdline)       /* EOF (ex: ctrl-d) */
            exit (EXIT_SUCCESS);

        P = parse_cmdline (cmdline);
        if (!P)
            goto next;

        if (P->invalid_syntax) {
            printf ("pssh: invalid syntax\n");
            goto next;
        }

#if DEBUG_PARSE
        parse_debug (P);
#endif

        execute_tasks (P);

    next:
        parse_destroy (&P);
        free(cmdline);
    }
}
