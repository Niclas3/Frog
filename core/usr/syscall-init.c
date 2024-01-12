#include <debug.h>
#include <ostype.h>
#include <print.h>
#include <stdio.h>
#include <string.h>

#include <sys/syscall-init.h>
#include <sys/syscall.h>

#include <fs/fs.h>  // for sys_write/ sys_open/ sys_close
#include <sys/exec.h>
#include <sys/fork.h>
#include <sys/exit.h>
#include <sys/graphic.h>
#include <sys/threads.h>
#include <fs/pipe.h>
#include <device/cmos.h>

#include <ipc.h>

// for test
#include <math.h>
#include <sys/memory.h>

#define syscall_max_nr 32
typedef void *syscall;

syscall syscall_table[syscall_max_nr];

uint_32 sys_getpid(void)
{
    return running_thread()->pid;
}

/**
 * The real producer of system call 'sendrec()'
 *
 * @param func SEND or RECEIVE
 * @param src_dest To/From whom the message is transferred. this is task number
 * @param p_msg pointer to message
 *
 * @return Zero if success
 *****************************************************************************/
uint_32 sys_sendrec(uint_32 func, uint_32 src_dest, message *p_msg)
{
    TCB_t *caller = running_thread();

    ASSERT((src_dest > 0 && src_dest < TASK_MAX) || src_dest == ANY_TASK ||
           src_dest == INTR_TASK);
    int ret = 0;
    p_msg->m_source = caller->pid;
    ASSERT(p_msg->m_source != src_dest);
    /**
     * There are three function about sending and receiving message, SEND,
     * RECIEVE, and BOTH. First two are easy understand.
     * BOTH mean it is transformed into a SEND followed by a RECIEVE
     *
     *****************************************************************************/
    if (func == SEND) {
        ret = msg_send(caller, src_dest, p_msg);
        if (ret != 0) {
            return ret;
        }
    } else if (func == RECEIVE) {
        ret = msg_receive(caller, src_dest, p_msg);
        if (ret != 0) {
            return ret;
        }
    } else if (func == BOTH) {
        ret = msg_send(caller, src_dest, p_msg);
        if (ret == 0)
            ret = msg_receive(caller, src_dest, p_msg);
    } else {
        char error[60];
        sprintf(error,
                "sys_sendrec invalid function: %d (SEND:%d, RECEIVE:%d).", func,
                SEND, RECEIVE);
        PANIC(error);
    }

    return 0;
}

void sys_putc(char c)
{
    put_char(c);
}

void sys_testsyscall(int a)
{
    return;
}

void syscall_init(void)
{
    // Mono-kernel way
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;

    syscall_table[SYS_FORK] = sys_fork;
    syscall_table[SYS_EXECV] = sys_execv;
    syscall_table[SYS_EXIT] = sys_exit;
    syscall_table[SYS_WAIT] = sys_wait;
    syscall_table[SYS_PIPE] = sys_pipe;

    syscall_table[SYS_OPEN] = sys_open;
    syscall_table[SYS_CLOSE] = sys_close;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_SEEK] = sys_lseek;
    syscall_table[SYS_UNLINK] = sys_unlink;
    syscall_table[SYS_MKDIR] = sys_mkdir;
    syscall_table[SYS_OPENDIR] = sys_opendir;
    syscall_table[SYS_CLOSEDIR] = sys_closedir;
    syscall_table[SYS_READDIR] = sys_readdir;
    syscall_table[SYS_REWINDDIR] = sys_rewinddir;
    syscall_table[SYS_RMDIR] = sys_rmdir;

    syscall_table[SYS_GETCWD] = sys_getcwd;
    syscall_table[SYS_CHDIR] = sys_chdir;
    syscall_table[SYS_STAT] = sys_stat;

    syscall_table[SYS_PUTC] = sys_putc;

    // mico-kernel way
    syscall_table[SYS_SENDREC] = sys_sendrec;
    // for test
    syscall_table[SYS_TESTSYSCALL] = sys_testsyscall;
    // timer
    syscall_table[SYS_GETTIMEOFDAY] = sys_gettimeofday;
    syscall_table[SYS_SETTIMEOFDAY] = sys_settimeofday;
}
