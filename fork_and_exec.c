#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h> /* pid_t                   */
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()*/
#include <unistd.h>    /* execvp(), fork()        */

#include "parse.h"     /* Task struct */

int execute_cmd(Task task)
{
    pid_t pid;

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "error -- failed to fork()");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        /* only executed by the PARENT process */
        int child_ret;
        waitpid(pid, &child_ret, 0);
    } 
    
    else {
        if(execvp(task.cmd, task.argv)) {
            printf("Failed to exec %s\n", task.cmd);
            exit(EXIT_FAILURE);
        }
    }

    return EXIT_SUCCESS;
}
