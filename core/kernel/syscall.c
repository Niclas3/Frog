#include <frog/syscall.h>
#include <asm/i386_syscall_common.h>

uint_32 getpid(void)
{
    return _syscall0(SYS_GETPID);
}

void *malloc(uint_32 size)
{
    return (void *) _syscall1(SYS_MALLOC, size);
}
void free(void *ptr)
{
    _syscall1(SYS_FREE, ptr);
}

uint_32 fork(void)
{
    return _syscall0(SYS_FORK);
}

void exit(int_32 status)
{
    _syscall1(SYS_EXIT, status);
}

pid_t wait(int_32 *status_loc)
{
    return _syscall1(SYS_WAIT, status_loc);
}

int_32 execv(const char *path, const char *argv[])
{
    return _syscall2(SYS_EXECV, path, argv);
}

int_32 pipe(int_32 pipefd[2])
{
    return _syscall1(SYS_PIPE, pipefd);
}

uint_32 wait2(int n, int_32 *fds, struct timeval *tvp){
    return _syscall3(SYS_WAIT2, n, fds, tvp);
}

int_32 open(const char *pathname, uint_8 flags)
{
    return _syscall2(SYS_OPEN, (uint_32) pathname, flags);
}
int_32 close(int_32 fd)
{
    return _syscall1(SYS_CLOSE, fd);
}
uint_32 write(int_32 fd, const void *buf, uint_32 count)
{
    return _syscall3(SYS_WRITE, fd, buf, count);
}

int_32 read(int_32 fd, void *buf, uint_32 count)
{
    return _syscall3(SYS_READ, fd, buf, count);
}

int_32 lseek(int_32 fd, int_32 offset, uint_8 whence)
{
    return _syscall3(SYS_SEEK, fd, offset, whence);
}

int_32 unlink(const char *pathname)
{
    return _syscall1(SYS_UNLINK, pathname);
}

int_32 mkdir(const char *pathname)
{
    return _syscall1(SYS_MKDIR, pathname);
}

struct dir *opendir(const char *name)
{
    return _syscall1(SYS_OPENDIR, name);
}

int_32 closedir(struct dir *dirp)
{
    return _syscall1(SYS_CLOSEDIR, dirp);
}

struct dir_entry *readdir(struct dir *dirp)
{
    return _syscall1(SYS_READDIR, dirp);
}
void rewinddir(struct dir *dirp)
{
    _syscall1(SYS_REWINDDIR, dirp);
}

int_32 rmdir(const char *pathname)
{
    return _syscall1(SYS_RMDIR, pathname);
}

char *getcwd(char *buf, int_32 size)
{
    return _syscall2(SYS_GETCWD, buf, size);
}

int_32 chdir(const char *pathname)
{
    return _syscall1(SYS_CHDIR, pathname);
}

int_32 stat(const char *pathname, struct stat *statbuf)
{
    return _syscall2(SYS_STAT, pathname, statbuf);
}

uint_32 ioctl(int_32 fd, uint_32 request, void* argp){
    return _syscall3(SYS_IOCTL, fd, request, argp);
}

void putc(char c)
{
    _syscall1(SYS_PUTC, c);
}

uint_32 sendrec(uint_32 func, uint_32 src_dest, message *p_msg)
{
    return _syscall3(SYS_SENDREC, func, src_dest, p_msg);
}

int gettimeofday(struct timeval *t, void *z)
{
    return _syscall2(SYS_GETTIMEOFDAY, t, z);
}

int settimeofday(struct timeval *t, void *z)
{
    return _syscall2(SYS_SETTIMEOFDAY, t, z);
}

void testsyscall(int a)
{
    _syscall1(SYS_TESTSYSCALL, a);
}
