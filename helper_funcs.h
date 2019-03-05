#ifndef _helper_funcs__h_
#define _helper_funcs__h_

int open_file(char *file);

void dup_files(int fd1, int fd2);

void set_pgid(pid_t pid, pid_t pgid);

#endif
