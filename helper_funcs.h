#ifndef _fork_and_exec_h_
#define _fork_and_exec_h_

#include "parse.h"

#define READ_SIDE 0
#define WRITE_SIDE 1
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()*/

int check_and_redirect_output(char *outfile);

int check_and_redirect_input(char *infile);

#endif
