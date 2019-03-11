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

/* Executes the task(s) of a user specified job
 *
 * Iterates through the number of tasks checking if
 * the function is a pssh built-in or POSIX standard
 * function. 
 *
 * Forks on successful cmd inputs and uses
 * pipes for interprocess communication. 
 *
 * If user specifies input or output file redirection, 
 * the function opens those files and reads/writes to them
 * accordingly. 
 *
 * Creates a job struct each time execute_tasks() is called 
 * and associates it with lowest available job number.
 *
 * @param  P Parse struct containing job info
 *
 * @return none
 * */
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

    new_job_num = assign_lowest_job_num();

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
            /* TODO Handle backgrounded built-in functions */
            dup2(output_fd, STDOUT_FILENO);
            builtin_execute(P->tasks[t]);
        }
        else if (command_found(P->tasks[t].cmd)) {
            pid[t] = fork();
            setpgid(pid[t], pid[0]);

            /* Parent process */
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

    free(pid);

    /* Restore stdin & out */
    dup2(og_stdin, STDIN_FILENO);
    dup2(og_stdout, STDOUT_FILENO);

    close(og_stdin);
    close(og_stdout);
}

/* SIGTTOU Handler
 *
 * Pauses until shell has control of STDOUT
 *
 * @param sig signal number
 *
 * @return none */
void handler_sigttou (int sig)
{
    while (tcgetpgrp(STDOUT_FILENO) != getpid ())
        pause ();
}

/* SIGTTIN Handler
 *
 * Pauses until shell has control on STDIN
 *
 * @param sig signal number
 *
 * @return none */
void handler_sigttin (int sig)
{
    while (tcgetpgrp(STDIN_FILENO) != getpid ())
        pause ();
}

/* SIGCHLD Handler 
 *
 * Waits on children fork'ed from shell and
 * handles their status (i.e. SIGTSTP, SIGCONT, etc) 
 *
 * @param sig signal number
 *
 * @return none */
void handler(int sig)
{
    pid_t child_pid;
    int status;
    int job_index;
    static int killed[MAX_JOBS] = {0};

    child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);

    job_index = find_job_index(child_pid);

    if (WIFSTOPPED(status)) {
        if (jobs[job_index].status == BG)
            jobs[job_index].status = STOPPED;

        /* If a job is in foreground and stopped:
         *  - set shell to foreground
         *  - tell user job is stopped */
        else if (jobs[job_index].status == FG) {
            set_fg_pgid(getpgrp());
            jobs[job_index].status = STOPPED;
            print_job_info(job_index, &jobs[job_index], 0);
        }
    }
    else if (WIFCONTINUED(status)) {
        fprintf(stdout, "[%d]+ Continued \t%s\n", job_index, jobs[job_index].name);
    }
    else {
        /* Check if job has had all of its children terminated:
         * If job complete:
         *  - set shell to foreground
         * If FG job:
         *  - remove job
         * If BG job:
         *  - send signal to main print and destroy job */
        killed[job_index]++;

        if (killed[job_index] == jobs[job_index].npids) {
            set_fg_pgid(getpgrp());
            killed[job_index] = 0;

            if (jobs[job_index].status == FG)
                destroy_job(&jobs[job_index]);
            else {
                bg_job_finished = 1;
                finished_job_num = job_index;
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

    init_jobs();

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
