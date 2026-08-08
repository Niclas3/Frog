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
#include <frog/exec.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/test.h>
#include <kernel/debug.h>
#include <kernel/framebuffer.h>
#include <kernel/mm_test.h>
#include <kernel/syscall_fs.h>
#include <kernel/timekeeping.h>
#include <kernel/qemu_test.h>
#include <kernel/wait2.h>

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

static int_32 sys_test_sync(uint_32 command)
{
#ifdef CONFIG_FROG_TEST_FRAMEBUFFER_MMAP
    unsigned long entry_flags;
    int result;

    local_irq_save(entry_flags);
    local_irq_enable();
    result = pc_framebuffer_test_verify_cleanup();

    if (result != 0) {
        frog_test_case("framebuffer.mmap.cleanup", 0);
        local_irq_restore(entry_flags);
        return result;
    }
    if (frog_test_has_failures()) {
        local_irq_restore(entry_flags);
        return -EUCLEAN;
    }

    frog_test_sync("framebuffer-mmap-ready");
    local_irq_disable();
    for (;;)
        __asm__ volatile("hlt");
#elif defined(CONFIG_FROG_TEST_INPUT)
    switch (command) {
    case FROG_TEST_INPUT_KEYBOARD_READY:
        frog_test_sync("input-keyboard-ready");
        return 0;
    case FROG_TEST_INPUT_MOUSE_MOVE_READY:
        frog_test_sync("input-mouse-move-ready");
        return 0;
    case FROG_TEST_INPUT_MOUSE_BUTTON_READY:
        frog_test_sync("input-mouse-button-ready");
        return 0;
    case FROG_TEST_INPUT_BOTH_READY:
        frog_test_sync("input-both-ready");
        return 0;
    default:
        return -EINVAL;
    }
#elif defined(CONFIG_FROG_TEST_WAIT2)
    return wait2_test_command(command);
#else
    (void) command;
    return -EOPNOTSUPP;
#endif
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
    case FROG_TEST_VM_FORK_ROLLBACK:
        name = "vm.fork-rollback";
        break;
    case FROG_TEST_VM_FORK_SHARED:
        name = "vm.fork-shared";
        break;
    case FROG_TEST_VM_EXIT_CHILD_FIRST:
        name = "vm.exit-child-first";
        break;
    case FROG_TEST_VM_EXIT_PARENT_FIRST:
        name = "vm.exit-parent-first";
        break;
    case FROG_TEST_VM_POST_UNMAP_FAULT:
        name = "vm.post-unmap-fault";
        break;
    case FROG_TEST_VM_MMAP_POINTER:
        name = "vm.mmap-pointer";
        break;
    case FROG_TEST_VM_MMAP_ARGUMENTS:
        name = "vm.mmap-arguments";
        break;
    case FROG_TEST_VM_MMAP_FD:
        name = "vm.mmap-fd";
        break;
    case FROG_TEST_VM_MMAP_ACCESS:
        name = "vm.mmap-access";
        break;
    case FROG_TEST_VM_MMAP_UNSUPPORTED:
        name = "vm.mmap-unsupported";
        break;
    case FROG_TEST_VM_MUNMAP_EXACT:
        name = "vm.munmap-exact";
        break;
    case FROG_TEST_VM_FILE_AFTER_CLOSE:
        name = "vm.file-after-close";
        break;
    case FROG_TEST_VM_MMAP_BUSY:
        name = "vm.mmap-busy";
        break;
    case FROG_TEST_PROCESS_WAIT_FAULT_RETRY:
        name = "process.wait-fault-retry";
        break;
    case FROG_TEST_PROCESS_ZOMBIE_ADOPTION:
        name = "process.zombie-adoption";
        break;
    case FROG_TEST_PROCESS_HEAP_FORK:
        name = "process.heap-fork";
        break;
    case FROG_TEST_EXEC_BAD_PATH:
        name = "exec.bad-path";
        break;
    case FROG_TEST_EXEC_BAD_POINTERS:
        name = "exec.bad-pointers";
        break;
    case FROG_TEST_EXEC_ARG_OVERFLOW:
        name = "exec.arg-overflow";
        break;
    case FROG_TEST_EXEC_INVALID_ELF:
        name = "exec.invalid-elf";
        break;
    case FROG_TEST_EXEC_FAILURE_PRESERVES:
        name = "exec.failure-preserves-image";
        break;
    case FROG_TEST_EXEC_SUCCESS:
        name = "exec.success";
        break;
    case FROG_TEST_EXEC_FD_INHERIT:
        name = "exec.fd-inherit";
        break;
    case FROG_TEST_INPUT_NONBLOCK:
        name = "input.nonblock";
        break;
    case FROG_TEST_INPUT_KEYBOARD:
        name = "input.keyboard";
        break;
    case FROG_TEST_INPUT_MOUSE_MOVE:
        name = "input.mouse-move";
        break;
    case FROG_TEST_INPUT_MOUSE_BUTTON:
        name = "input.mouse-button";
        break;
    case FROG_TEST_INPUT_WAIT2_EMPTY:
        name = "input.wait2.empty";
        break;
    case FROG_TEST_INPUT_WAIT2_KEYBOARD:
        name = "input.wait2.keyboard";
        break;
    case FROG_TEST_INPUT_WAIT2_MOUSE_MOVE:
        name = "input.wait2.mouse-move";
        break;
    case FROG_TEST_INPUT_WAIT2_MOUSE_BUTTON:
        name = "input.wait2.mouse-button";
        break;
    case FROG_TEST_INPUT_WAIT2_BOTH:
        name = "input.wait2.both";
        break;
    case FROG_TEST_INPUT_WAIT2_CLOSE:
        name = "input.wait2.close";
        break;
    case FROG_TEST_TIME_MONOTONIC_NORMALIZED:
        name = "time.monotonic.normalized";
        break;
    case FROG_TEST_TIME_MONOTONIC_ADVANCES:
        name = "time.monotonic.advances";
        break;
    case FROG_TEST_TIME_MONOTONIC_BAD_POINTERS:
        name = "time.monotonic.bad-pointers";
        break;
    case FROG_TEST_TIME_INVALID_CLOCK:
        name = "time.clock.invalid";
        break;
    case FROG_TEST_TIME_REALTIME_UNAVAILABLE:
        name = "time.realtime.unavailable";
        break;
    case FROG_TEST_TIME_SETTIMEOFDAY_UNIMPLEMENTED:
        name = "time.settimeofday-unimplemented";
        break;
    case FROG_TEST_WAIT2_AVAILABLE:
        name = "wait2.available";
        break;
    case FROG_TEST_WAIT2_ABI:
        name = "wait2.abi";
        break;
    case FROG_TEST_WAIT2_ARGUMENTS:
        name = "wait2.arguments";
        break;
    case FROG_TEST_WAIT2_NEGATIVE_FD:
        name = "wait2.negative-fd";
        break;
    case FROG_TEST_WAIT2_INVALID_FD:
        name = "wait2.invalid-fd";
        break;
    case FROG_TEST_WAIT2_REVENTS_RESET:
        name = "wait2.revents-reset";
        break;
    case FROG_TEST_WAIT2_TIMEOUT_ZERO:
        name = "wait2.timeout-zero";
        break;
    case FROG_TEST_WAIT2_FINITE_SLEEP:
        name = "wait2.finite-sleep";
        break;
    case FROG_TEST_WAIT2_BOUNDARY_SLEEP:
        name = "wait2.boundary-sleep";
        break;
    case FROG_TEST_WAIT2_MULTIPLE_INVALID:
        name = "wait2.multiple-invalid";
        break;
    case FROG_TEST_WAIT2_COPYOUT_CLEANUP:
        name = "wait2.copyout-cleanup";
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
    if (command == FROG_TEST_HEAP_ALLOC_16)
        return (int_32) (uint_32) umalloc(16);
    if (command == FROG_TEST_VM_PREPARE)
        return mm_vm_process_prepare();
    if (command == FROG_TEST_VM_VERIFY_CLEANUP)
        return mm_vm_process_verify_cleanup();
    if (command >= FROG_TEST_VM_FORK_FAIL_BASE &&
        command < FROG_TEST_VM_FORK_FAIL_BASE + FROG_TEST_VM_FORK_FAIL_COUNT)
        return mm_vm_process_arm_fork_failure(
            command - FROG_TEST_VM_FORK_FAIL_BASE);
    if (command > FROG_TEST_VM_VERIFY_REFS_BASE &&
        command <= FROG_TEST_VM_VERIFY_REFS_BASE +
                       FROG_TEST_VM_FORK_FAIL_COUNT)
        return mm_vm_process_verify_refs(
            command - FROG_TEST_VM_VERIFY_REFS_BASE);
#endif
#ifdef CONFIG_FROG_TEST_DISK
    if (command == FROG_TEST_EXEC_FAIL_PRECOMMIT)
        return exec_test_arm_fail_before_commit();
#endif
#if !defined(CONFIG_FROG_TEST_USER) && \
    !defined(CONFIG_FROG_TEST_FRAMEBUFFER_MMAP) && \
    !defined(CONFIG_FROG_TEST_INPUT)
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
    syscall_table[SYS_MMAP]    = sys_mmap;
    syscall_table[SYS_MUNMAP]  = sys_munmap;
    syscall_table[SYS_CLOCK_GETTIME] = sys_clock_gettime;
    syscall_table[SYS_GETTIMEOFDAY] = sys_gettimeofday;
    syscall_table[SYS_SETTIMEOFDAY] = sys_settimeofday;
    syscall_table[SYS_TEST_SYNC] = sys_test_sync;
    syscall_table[SYS_FORK]    = sys_fork;
    syscall_table[SYS_EXIT]    = sys_exit;
    syscall_table[SYS_EXECV]   = sys_execv;
    syscall_table[SYS_WAIT]    = sys_wait;
    syscall_table[SYS_WAIT2]   = sys_wait2;
    syscall_table[SYS_TESTSYSCALL] = sys_testsyscall;
#ifdef CONFIG_QEMU_TEST
    syscall_table[SYS_TEST_REPORT] = sys_test_report;
#endif
}
