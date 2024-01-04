#ifndef __SYS_SYSCALL_H
#define __SYS_SYSCALL_H
#include <ostype.h>
#include <ipc.h>
#include <fs/fs.h>

typedef struct kwaak_msg message;

enum SYSCALL_NR{
    SYS_GETPID, // 0
    SYS_SENDREC,
    //heap
    SYS_MALLOC,
    SYS_FREE,
    //process
    SYS_FORK,
    SYS_EXECV,
    SYS_WAIT,
    SYS_EXIT,
    //file system api
    SYS_OPEN,
    SYS_CLOSE,
    SYS_READ,
    SYS_WRITE,
    SYS_SEEK,
    SYS_UNLINK,
    SYS_MKDIR,
    SYS_OPENDIR,
    SYS_CLOSEDIR,
    SYS_READDIR,
    SYS_REWINDDIR,
    SYS_RMDIR,
    SYS_GETCWD,
    SYS_CHDIR,
    SYS_STAT,
    SYS_PIPE,
    //i/o
    SYS_PUTC,
    // for test
    SYS_TESTSYSCALL,
};

/**
 * TODO:
  All system calls
 +------------------------------------------------------------------------------------------------------------
 |                    +--+---------------------------------------------------------------------------------------
 | process management |  | 01. pid = fork()                      | create a child process identical to the parent
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 02. pid = waitpid(pid, &statloc, opts)| wait for a child to terminate
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 03. s = wait(&status)                 | old waitpid()
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 04. s = execve(name, argv, envp)      | Replace a process core image
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 05. exit(status)                      | Terminate process execution and return status
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |* | 06. size = brk(addr)                  | Set the size of the data segment
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    | @| 07. pid = getpid()                    | Return the caller's process id
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 08. pid = getpgrp()                   | Return the id of the caller's process group
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 09. pid = setsid()                    | Create a new session and return its proc. group id
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 10. l = ptrace(req, pid, addr, data)  | Used for debugging
 |                    +--+---------------------------------------+-----------------------------------------------
 +------------------------------------------------------------------------------------------------------------
 |                    +--+---------------------------------------------------------------------------------------
 |  File Management   |  | 01. fd = create(name, mode)           | Obsolete way to create a new file
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 02. fd = mknod(name, mode, addr)      | Create a regular, special, or directory i-node
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 03. fd = open(file, how,...)          | Open a file for reading, writing or both
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 04. fd = close(fd)                    | Colse an open file
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 05. n = read(fd, buffer, nbytes)      | Read data from a file into a buffer
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 06. n = write(fd, buffer, nbytes)     | Write data from a buffer into a file
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 07. pos = lseek(fd, offset, whence)   | Move the file pointer
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 08. s = stat(name, &buf)              | Get a file's status information
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 09. s = fstat(fd, &buf)               | Get a file's status information
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 10. fd = dup(fd)                      | Allocate a new file descriptor for an open file
 |                    +--+---------------------------------------------------------------------------------------
 |                    |  | 11. s = pipe(&fd[0])                  | Create a pipe
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 12. s = ioctl(fd, request, argp)      | Perform special operations on a file
 |                    +--+---------------------------------------+----------------------------------------------- |                    |  | 13. s = access(name, amode)           | Check a file's accessibility
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 14. s = rename(old, new)              | Give a file a new name
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 15. s = fcntl(fd, cmd, ...)           | File locking and other operation
 |                    +--+---------------------------------------+-----------------------------------------------
 +------------------------------------------------------------------------------------------------------------
 |  Dir. &            +--+---------------------------------------------------------------------------------------
 |  File Management   |  | 01. s = mkdir(name, mode)             | Create a new directory
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 02. s = rmdir(name)                   | Remove an empty directory
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 03. s = link(name1, name2)            | Create a new entry, name2, pointing to name1
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 04. s = unlink(name)                  | Remove a directory entry
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 05. s = mount(special, name, flag)    | Mount a file system
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 06. s = umount(special)               | Unmount a file system
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 07. s = sync()                        | Flush all cached blocks to the disk
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 08. s = chdir(dirname)                | Change the working directory
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 09. s = chroot(dirname)               | Change the root directory
 |                    +--+---------------------------------------+-----------------------------------------------
 +------------------------------------------------------------------------------------------------------------
 |    Protection      +--+---------------------------------------------------------------------------------------
 |                    |  | 01. s = chmod(name, mode)             | Change a file's protection bits
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 02. uid = getuid()                    | Get the caller's uid
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 03. gid = getgid()                    | Get the caller's gid
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 04. s = setuid()                      | set the caller's uid
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 05. s = setgid()                      | set the caller's gid
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 06. s = chown(name, owner, group)     | Change a file's owner and group
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 07. oldmask = umask(complmode)        | Change the mode mask
 |                    +--+---------------------------------------+-----------------------------------------------
 +------------------------------------------------------------------------------------------------------------
 | Time Management    +--+---------------------------------------------------------------------------------------
 |                    |  | 01. seconds = time(&seconds)          | Get the elapsed time sice Jan. 1, 1970
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 02. s = stime(tp)                     | Set the elapsed time since Jan. 1, 1970
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 03. s = utime(file, timep)            | Set a file's "last access" time
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 04. s = times(buffer)                 | Get the user and system times used so far
 |                    +--+---------------------------------------+-----------------------------------------------
 +------------------------------------------------------------------------------------------------------------
 |                    +--+---------------------------------------------------------------------------------------
 |  Signals           |  | 01. s = sigaction(sig, &act, &oldact) | Define action to take on signals
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 02. s = sigreturn(&context)           | Return from a signal
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 03. s = sigprocmask(how, &set, &old)  | Examine or change the signal mask
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 04. s = sigpending(set)               | Get the set of blocked signals
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 05. s = sigsuspend(sigmask)           | Replace the signal mask and suspend the process
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 06. s = kill(pid, sig)                | Send a signal to a process to kill a process
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 07. residual = alarm(seconds)         | Set the alarm clock
 |                    +--+---------------------------------------+-----------------------------------------------
 |                    |  | 08. s = pause()                       | Suspend the caller until the next signal
 |                    +--+---------------------------------------+-----------------------------------------------
 +------------------------------------------------------------------------------------------------------------
 *
 */
uint_32 getpid(void);

void* malloc(uint_32 size);
void free(void *ptr);

uint_32 fork(void);

int_32 execv(const char *path, const char *argv[]);

void exit(int_32 status);

pid_t wait(int_32 *status_loc);

int_32 pipe(int_32 pipefd[2]);

//System call: uint_32 write(char*)
//return len of str
int_32 open(const char *pathname, uint_8 flags);

int_32 close(int_32 fd);

uint_32 write(int_32 fd, const void *buf, uint_32 count);

int_32 read(int_32 fd, void *buf, uint_32 count);

int_32 lseek(int_32 fd, int_32 offset, uint_8 whence);

int_32 unlink(const char *pathname);

int_32 mkdir(const char *pathname);

struct dir *opendir(const char *name);

int_32 closedir(struct dir *dirp);

struct dir_entry *readdir(struct dir *dirp);
void rewinddir(struct dir *dirp);

int_32 rmdir(const char *pathname);

char *getcwd(char *buf, int_32 size);

int_32 chdir(const char *pathname);

int_32 stat(const char *pathname, struct stat *statbuf);

void putc(char c);
uint_32 sendrec(uint_32 func, uint_32 src_dest, message* p_msg);
void testsyscall(int a);

#endif
