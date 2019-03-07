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

#define READ_SIDE 0
#define WRITE_SIDE 1

Job job;

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P)
{
    int t = 0;
    int input_fd, output_fd;
    int pipe_fd[2];
    pid_t *pid = malloc(P->ntasks * sizeof(pid_t));

    int og_stdin = dup(STDIN_FILENO);
    int og_stdout = dup(STDOUT_FILENO);

    input_fd = og_stdin;

    if (P->infile) {
        input_fd = open(P->infile, O_RDONLY, 0644);
        dup2(input_fd, STDIN_FILENO);
    }

    for (t = 0; t < (P->ntasks-1); t++) {
        pipe(pipe_fd);
        output_fd = pipe_fd[WRITE_SIDE];

        if (is_builtin(P->tasks[t].cmd))
            builtin_execute(P->tasks[t]);

        else if (command_found(P->tasks[t].cmd)) {
            pid[t] = fork();
            setpgid(pid[t], pid[0]);
            set_fg_pgid(pid[0]);

            if (t == 0) {
                job.name = P->tasks[0].cmd;
                job.npids = P->ntasks;
                job.pgid = pid[0];
                job.pid[0] = job.pgid;
            }

            if (pid[t] == 0) {
                dup2(input_fd, STDIN_FILENO);
                dup2(output_fd, STDOUT_FILENO);
                execvp(P->tasks[t].cmd, P->tasks[t].argv);
            }

            else if (pid[t] > 0) {
                input_fd = pipe_fd[READ_SIDE];
                close(pipe_fd[WRITE_SIDE]);
                job.pid[t] = pid[t];
            }
        }

        else
            printf("pssh: does not exist\n");
    }

    output_fd = og_stdout;

    if (P->outfile)
        output_fd = open(P->outfile, O_CREAT | O_WRONLY, 0644);

    if (is_builtin(P->tasks[t].cmd))
        builtin_execute(P->tasks[t]);

    else if (command_found(P->tasks[t].cmd)) {
        pid[t] = fork();
        setpgid(pid[t], pid[0]);
        set_fg_pgid(pid[0]);

        job.pid[t] = pid[t];

        if (pid[t] == 0) {
            dup2(input_fd, STDIN_FILENO);
            dup2(output_fd, STDOUT_FILENO);
            execvp(P->tasks[t].cmd, P->tasks[t].argv);
        }
    }

    else 
        printf("pssh: does not exist\n");

    printf("JOB NAME: %s | PGID: %d | Num proc: %d\n", 
            job.name, job.pgid, job.npids);

    for (t = 0; t < P->ntasks; t++)
        printf("pid %d\n", job.pid[t]);

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
    int status;
    static int proc_killed;

    waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);

    if (WIFSTOPPED(status)) {
    }
    else if (WIFCONTINUED(status)) {
    }
    else {
        proc_killed++;

        if (proc_killed == job.npids)
            set_fg_pgid(getpgrp());
    }
}

int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;

    print_banner ();

    signal(SIGCHLD, handler);
    signal(SIGTTOU, handler_sigttou);
    signal(SIGTTIN, handler_sigttin);

    while (1) {
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

