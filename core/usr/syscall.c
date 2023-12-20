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

#define _syscall2(NUMBER, ARG1, ARG2)             \
    ({                                            \
        int __res;                                \
        __asm__ volatile("int $0x93"              \
                         : "=a"(__res)            \
                         : "a"(NUMBER), "b"(ARG2) \
                         : "memory");             \
        __res;                                    \
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
    return _syscall0(SYS_getpid);
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
    return (void *) _syscall1(SYS_malloc, size);
}
void free(void *ptr)
{
    _syscall1(SYS_free, ptr);
}

uint_32 fork(void)
{
    return _syscall0(SYS_fork);
}

int_32 open(const char *pathname, uint_8 flags)
{
    return _syscall2(SYS_open, pathname, flags);
}
int_32 close(int_32 fd)
{
    return _syscall1(SYS_close, fd);
}
uint_32 write(int_32 fd, const void *buf, uint_32 count)
{
    return _syscall3(SYS_write, fd, buf, count);
}

int_32 read(int_32 fd, void *buf, uint_32 count)
{
    return _syscall3(SYS_read, fd, buf, count);
}

int_32 lseek(int_32 fd, int_32 offset, uint_8 whence)
{
    return _syscall3(SYS_seek, fd, offset, whence);
}

int_32 unlink(const char *pathname)
{
    return _syscall1(SYS_unlink, pathname);
}

int_32 mkdir(const char *pathname)
{
    return _syscall1(SYS_mkdir, pathname);
}

struct dir *opendir(const char *name)
{
    return _syscall1(SYS_opendir, name);
}

int_32 closedir(struct dir *dirp)
{
    return _syscall1(SYS_closedir, dirp);
}

struct dir_entry *readdir(struct dir *dirp)
{
    return _syscall1(SYS_readdir, dirp);
}
void rewinddir(struct dir *dirp)
{
    _syscall1(SYS_rewinddir, dirp);
}

int_32 rmdir(const char *pathname)
{
    return _syscall1(SYS_rmdir, pathname);
}

uint_32 sendrec(uint_32 func, uint_32 src_dest, message *p_msg)
{
    return _syscall3(SYS_sendrec, func, src_dest, p_msg);
}
