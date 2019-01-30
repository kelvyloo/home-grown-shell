#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtin.h"
#include "parse.h"

static char* builtin[] = {
    "exit",   /* exits the shell */
    "which",  /* displays full path to command */
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

    for (tmp = PATH; ; tmp = NULL) {
        dir = strtok(tmp, ":");

        if (!dir) { break; }

        strncpy(probe, dir, PATH_MAX);
        strncat(probe, "/", PATH_MAX);
        strncat(probe, *(T.argv + 1) , PATH_MAX);

        if (access(probe, X_OK) == 0) {
            printf("%s\n", probe);
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
    if (!strcmp (T.cmd, "which")) {
        if (is_builtin(*(T.argv + 1))) {
            printf("%s: shell built-in command\n", *(T.argv + 1));
        }
        else {
            find_filepath(T);
        }
    }
    else {
        printf ("pssh: builtin command: %s (not implemented!)\n", T.cmd);
    }
}
