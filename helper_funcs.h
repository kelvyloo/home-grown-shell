#ifndef _helper_funcs__h_
#define _helper_funcs__h_

#include "parse.h"

int check_and_redirect_output(char *outfile);

int check_and_redirect_input(char *infile);

int execute_cmd(Parse *P, unsigned int t);

#endif
