#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <readline/readline.h>

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

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P)
{
    if (P->infile)
        input_fd = open(P->infile);

    for task in jobs-1
        pipe(fd);
        output_fd = write_pipe;

        if (builtin)
            execute_builtin();

        else if (command_found)
            pid = fork();

            if (child)
                dup2(input_fd, stdin);
                dup2(output_fd, stdout);
                execvp();

            else if (parent)
                read = prev_read;
                close(write_pipe);

        else
            printf("Does not exist");

    if (P->outfile)
        output_fd = open(P->infile);

    if (builtin)
        execute_builtin();

    else if (command_found)
        execvp();

    else 
        printf("Does not exist");
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

    waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED);

    if (WIFSTOPPED(status)) {
    }
    else if (WIFCONTINUED(status)) {
    }
    else {
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

