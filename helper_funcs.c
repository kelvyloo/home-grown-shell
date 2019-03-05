#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

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

void set_pgid(pid_t pid, pid_t pgid)
{
    if (setpgid(pid, pgid))
        fprintf(stderr, "pssh: error -- setpgid() failed\n");
}
