#include <fs/fs.h>
#include <sys/syscall.h>
#include <sys/threads.h>

/* X is 0–6, which are  the  number  of  arguments taken by the system call */
// NUMBER sub-routine number
/* argN is the name of the Nth argument */
#define _syscall0(NUMBER)                                                     \
    ({                                                                        \
        int __res;                                                            \
        __asm__ volatile("int $0x93" : "=a"(__res) : "a"(NUMBER) : "memory"); \
        __res;                                                                \
    });

#define _syscall1(NUMBER, ARG1)                   \
    ({                                            \
        int __res;                                \
        __asm__ volatile("int $0x93"              \
                         : "=a"(__res)            \
                         : "a"(NUMBER), "b"(ARG1) \
                         : "memory");             \
        __res;                                    \
    });

#define _syscall2(NUMBER, ARG1, ARG2)                        \
    ({                                                       \
        int __res;                                           \
        __asm__ volatile("int $0x93"                         \
                         : "=a"(__res)                       \
                         : "a"(NUMBER), "b"(ARG1), "c"(ARG2) \
                         : "memory");                        \
        __res;                                               \
    });


#define _syscall3(NUMBER, ARG1, ARG2, ARG3)                             \
    ({                                                                  \
        int __res;                                                      \
        __asm__ volatile("int $0x93"                                    \
                         : "=a"(__res)                                  \
                         : "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3) \
                         : "memory");                                   \
        __res;                                                          \
    });

#define _syscall4(NUMBER, ARG1, ARG2, ARG3, ARG4)                        \
    ({                                                                   \
        int __res;                                                       \
        __asm__ volatile("int $0x93"                                     \
                         : "=a"(__res)                                   \
                         : "a"(NUMBER), "b"(ARG1), "c"(ARG2), "d"(ARG3), \
                           "S"(ARG4)                                     \
                         : "memory");                                    \
        __res;                                                           \
    });

uint_32 getpid(void)
{
    return _syscall0(SYS_GETPID);
}



pid_t get_pid(void)
{
    message msg;
    reset_msg(&msg);
    msg.m_type = GET_PID;
    sendrec(BOTH, TASK_SYS, &msg);
    return msg.RETVAL;
}

// How to communicate with kernel code, like use interrupt
uint_32 get_ticks(void)
{
    message msg;
    reset_msg(&msg);
    msg.m_type = GET_TICKS;
    sendrec(BOTH, TASK_SYS, &msg);
    return msg.RETVAL;
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

char *sys_getcwd(char *buf, int_32 size)
{
    return _syscall2(SYS_GETCWD, buf, size);
}

int_32 sys_chdir(const char *pathname)
{
    return _syscall1(SYS_CHDIR, pathname);
}

int_32 sys_stat(const char *pathname, struct stat *statbuf)
{
    return _syscall2(SYS_STAT, pathname, statbuf);
}

uint_32 sendrec(uint_32 func, uint_32 src_dest, message *p_msg)
{
    return _syscall3(SYS_SENDREC, func, src_dest, p_msg);
}
