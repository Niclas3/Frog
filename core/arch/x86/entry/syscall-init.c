/* #include <debug.h> */
/* #include <frog/types.h> */
/* #include <print.h> */
/* #include <stdio.h> */
/* #include <string.h> */

#include <frog/syscall-init.h>
#include <frog/syscall.h>
#include <frog/exit.h>
#include <frog/fork.h>
#include <frog/errno.h>
#include <frog/test.h>
#include <kernel/debug.h>
#include <kernel/mm_test.h>
#include <kernel/syscall_fs.h>
#include <kernel/qemu_test.h>

/* #include <fs/fs.h>  // for sys_write/ sys_open/ sys_close */
/* #include <sys/exec.h> */
/* #include <sys/fork.h> */
/* #include <sys/exit.h> */
/* #include <sys/graphic.h> */
/* #include <sys/threads.h> */
/* #include <fs/pipe.h> */
/* #include <device/cmos.h> */
/* #include <fs/select.h> */
/* #include <ipc.h> */

// for test
/* #include <math.h> */
/* #include <sys/memory.h> */

typedef void *syscall;

syscall syscall_table[SYS_NR_COUNT];
uint_32 syscall_table_size = SYS_NR_COUNT;

static int_32 sys_ni_syscall(void)
{
    return -ENOSYS;
}

#ifdef CONFIG_QEMU_TEST
static int_32 sys_test_report(uint_32 id, int_32 passed)
{
    const char *name;

    switch (id) {
    case FROG_TEST_PROCESS_FORK_PARENT_RESULT:
        name = "process.fork.parent-result";
        break;
    case FROG_TEST_PROCESS_WAIT_PID:
        name = "process.wait.pid";
        break;
    case FROG_TEST_PROCESS_WAIT_STATUS:
        name = "process.wait.status";
        break;
    case FROG_TEST_PROCESS_FORK_ADDRESS_SPACE:
        name = "process.fork.address-space";
        break;
    case FROG_TEST_PROCESS_WAIT_NO_CHILD:
        name = "process.wait.no-child";
        break;
    case FROG_TEST_FD_FORK_CHILD_CLOSE:
        name = "fd.fork-child-close";
        break;
    case FROG_TEST_FD_FORK_PARENT_CLOSE:
        name = "fd.fork-parent-close";
        break;
    case FROG_TEST_SYSCALL_OUT_OF_RANGE:
        name = "syscall.out-of-range";
        break;
    case FROG_TEST_SYSCALL_UNIMPLEMENTED:
        name = "syscall.unimplemented";
        break;
    case FROG_TEST_VM_USER_ADDRESS:
        name = "vm.user-address";
        break;
    case FROG_TEST_VM_USER_READ:
        name = "vm.user-read";
        break;
    case FROG_TEST_VM_USER_WRITE:
        name = "vm.user-write";
        break;
    case FROG_TEST_VM_CLEANUP:
        name = "vm.cleanup";
        break;
    default:
        return -EINVAL;
    }

    frog_test_case(name, passed != 0);
    return 0;
}
#endif

/* uint_32 sys_getpid(void) */
/* { */
/*     return running_thread()->pid; */
/* } */
/*  */
/* #<{(|* */
/*  * The real producer of system call 'sendrec()' */
/*  * */
/*  * @param func SEND or RECEIVE */
/*  * @param src_dest To/From whom the message is transferred. this is task number */
/*  * @param p_msg pointer to message */
/*  * */
/*  * @return Zero if success */
/*  ****************************************************************************|)}># */
/* uint_32 sys_sendrec(uint_32 func, uint_32 src_dest, message *p_msg) */
/* { */
/*     TCB_t *caller = running_thread(); */
/*  */
/*     ASSERT((src_dest > 0 && src_dest < TASK_MAX) || src_dest == ANY_TASK || */
/*            src_dest == INTR_TASK); */
/*     int ret = 0; */
/*     p_msg->m_source = caller->pid; */
/*     ASSERT(p_msg->m_source != src_dest); */
/*     #<{(|* */
/*      * There are three function about sending and receiving message, SEND, */
/*      * RECIEVE, and BOTH. First two are easy understand. */
/*      * BOTH mean it is transformed into a SEND followed by a RECIEVE */
/*      * */
/*      ****************************************************************************|)}># */
/*     if (func == SEND) { */
/*         ret = msg_send(caller, src_dest, p_msg); */
/*         if (ret != 0) { */
/*             return ret; */
/*         } */
/*     } else if (func == RECEIVE) { */
/*         ret = msg_receive(caller, src_dest, p_msg); */
/*         if (ret != 0) { */
/*             return ret; */
/*         } */
/*     } else if (func == BOTH) { */
/*         ret = msg_send(caller, src_dest, p_msg); */
/*         if (ret == 0) */
/*             ret = msg_receive(caller, src_dest, p_msg); */
/*     } else { */
/*         char error[60]; */
/*         sprintf(error, */
/*                 "sys_sendrec invalid function: %d (SEND:%d, RECEIVE:%d).", func, */
/*                 SEND, RECEIVE); */
/*         PANIC(error); */
/*     } */
/*  */
/*     return 0; */
/* } */
/*  */
/* void sys_putc(char c) */
/* { */
/*     put_char(c); */
/* } */

int_32 sys_testsyscall(uint_32 command)
{
#ifdef CONFIG_FROG_TEST_PROCESS
    if (command == FROG_TEST_VM_PREPARE)
        return mm_vm_process_prepare();
    if (command == FROG_TEST_VM_VERIFY_CLEANUP)
        return mm_vm_process_verify_cleanup();
#endif
#ifndef CONFIG_FROG_TEST_USER
    INFO("[init]: ring3 reached, testsyscall a=%d", command);
#endif
#ifdef CONFIG_QEMU_TEST
#ifdef CONFIG_FROG_TEST_USER
    frog_test_case("user.low-image.syscalls", command == 0x46524f47);
#endif
#ifdef CONFIG_FROG_TEST_PROCESS
    mm_uaccess_process_regression();
#endif
    frog_test_milestone("ring3", 1);
    frog_test_finish();
#endif
    return 0;
}

void syscall_init(void)
{
    for (uint_32 nr = 0; nr < SYS_NR_COUNT; nr++)
        syscall_table[nr] = sys_ni_syscall;

    syscall_table[SYS_OPEN]    = sys_open;
    syscall_table[SYS_CLOSE]   = sys_close;
    syscall_table[SYS_READ]    = sys_read;
    syscall_table[SYS_WRITE]   = sys_write;
    syscall_table[SYS_SEEK]    = sys_lseek;
    syscall_table[SYS_UNLINK]  = sys_unlink;
    syscall_table[SYS_MKDIR]   = sys_mkdir;
    syscall_table[SYS_RMDIR]   = sys_rmdir;
    syscall_table[SYS_IOCTL]   = sys_ioctl;
    syscall_table[SYS_FORK]    = sys_fork;
    syscall_table[SYS_EXIT]    = sys_exit;
    syscall_table[SYS_WAIT]    = sys_wait;
    syscall_table[SYS_TESTSYSCALL] = sys_testsyscall;
#ifdef CONFIG_QEMU_TEST
    syscall_table[SYS_TEST_REPORT] = sys_test_report;
#endif
}
