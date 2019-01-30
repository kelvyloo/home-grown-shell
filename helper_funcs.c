#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* pid_t                   */
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()*/
#include <unistd.h>    /* execvp(), fork()        */

#include "parse.h"     /* Task struct */
#include <fcntl.h>     /* open()      */

#define READ_SIDE 0
#define WRITE_SIDE 1

int check_and_redirect_output(char *outfile) 
{
    unsigned int out_fd = 0;

    if (outfile != NULL) {
        out_fd = open(outfile, O_CREAT | O_RDONLY | O_WRONLY, 0644);
        
        if (out_fd == -1) {
            printf("pssh: failed to open/create file %s\n", outfile);
            return EXIT_FAILURE;
        }

        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            printf("pssh: failed to redirect output to %s\n", outfile);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

int check_and_redirect_input(char *infile) 
{
    unsigned int in_fd = 0;

    if (infile != NULL) {
        in_fd = open(infile, O_RDONLY);
        
        if (in_fd == -1) {
            printf("pssh: failed to open/create file %s\n", infile);
            return EXIT_FAILURE;
        }

        if (dup2(in_fd, STDIN_FILENO) == -1) {
            printf("pssh: failed to redirect input to %s\n", infile);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
