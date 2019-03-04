#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "parse.h"

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
    "kill",   /* send signal to process */
    NULL
};


int is_builtin (char* cmd)
{
    int i;

    for (i=0; builtin[i]; i++) {
        if (!strcmp (cmd, builtin[i]))
            return 1;
    }

    return 0;
}

void find_filepath(Task T)
{
    char *PATH;
    char *tmp;
    char *dir;
    char probe[PATH_MAX];

    PATH = strdup(getenv("PATH"));

    /* Look in the PATH env directories for cmd specified */
    for (tmp = PATH; ; tmp = NULL) {
        dir = strtok(tmp, ":");

        if (!dir) { break; }

        /* Build string to test */
        strncpy(probe, dir, PATH_MAX);
        strncat(probe, "/", PATH_MAX);
        strncat(probe, *(T.argv + 1) , PATH_MAX);

        if (access(probe, X_OK) == 0) {
            fprintf(stdout, "%s\n", probe);
            break;
        }
    }

    free(PATH);
}

void builtin_execute (Task T)
{
    if (!strcmp (T.cmd, "exit")) {
        exit (EXIT_SUCCESS);
    }
    else if (!strcmp (T.cmd, "which")) {
        if (*(T.argv + 1) != NULL) {
            if (is_builtin(*(T.argv + 1))) {
                fprintf(stdout, "%s: shell built-in command\n", *(T.argv + 1));
            }
            else {
                find_filepath(T);
            }
        }
    }
    else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}
