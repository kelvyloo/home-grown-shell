#ifndef _helper_funcs__h_
#define _helper_funcs__h_

#include "parse.h"

#define READ_SIDE 0
#define WRITE_SIDE 1
#include <sys/wait.h>  /* waitpid(), WEXITSTATUS()*/

int check_and_redirect_output(char *outfile);

int check_and_redirect_input(char *infile);

#endif
