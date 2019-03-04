#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>

#include "builtin.h"
#include "parse.h"

#include "helper_funcs.h"

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
        printf("%s", cwd);
    }
    free(cwd);

    return  "$ ";
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

#include <sys/wait.h>
#include <fcntl.h>     /* open()      */

#define READ_SIDE 0
#define WRITE_SIDE 1

int open_file(char *file)
{
    int fd;

    fd = open(file, O_CREAT | O_RDWR, 0644);
    
    if (fd == -1)
        fprintf(stderr, "pssh: failed to open file %s\n", file);

    return fd;
}

void dup_files(int fd1, int fd2)
{
    if (dup2(fd1, fd2) == -1)
        fprintf(stderr, "pssh: error -- dup2() failed\n");
}

/* Called upon receiving a successful parse.
 * This function is responsible for cycling through the
 * tasks, and forking, executing, etc as necessary to get
 * the job done! */
void execute_tasks (Parse* P)
{
    unsigned int t;
    int pipe_fd[2] = {0};
    int input_fd = 0;
    int out_fd = 0;
    pid_t pid;

    if (P->infile != NULL)
        input_fd = open_file(P->infile);

    for (t = 0; t < P->ntasks; t++) {
        if (pipe(pipe_fd) == -1) {
            fprintf(stderr, "pssh: failed to create pipe\n");
            break;
        }

        pid = fork();

        if (pid < 0) {
            fprintf(stderr, "pssh: fork failed\n");
            break;
        }
        else if (pid > 0) {
            wait(NULL);
            close(pipe_fd[WRITE_SIDE]);
             // Hold previous read fd for next task in multipipe
            input_fd = pipe_fd[READ_SIDE];
        }
        else {
            dup_files(input_fd, STDIN_FILENO);

            /* If task is not the last one -> write to pipe not stdout */
            if (t < P->ntasks-1)
                dup_files(pipe_fd[WRITE_SIDE], STDOUT_FILENO);

            if (t == P->ntasks-1 && (P->outfile != NULL)) {
                out_fd = open_file(P->outfile);

                dup_files(out_fd, STDOUT_FILENO);
            }

            if (is_builtin (P->tasks[t].cmd)) {
                builtin_execute(P->tasks[t]);
            }
            else if (command_found (P->tasks[t].cmd)) {
                execvp(P->tasks[t].cmd, P->tasks[t].argv);
            }
            else {
                fprintf (stderr, "pssh: command not found: %s\n", P->tasks[t].cmd);
                break;
            }
        }
    }
}


int main (int argc, char** argv)
{
    char* cmdline;
    Parse* P;

    print_banner ();

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

