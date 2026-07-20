#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

#define USER_SMOKE_MAGIC 0x46524f47U
#define USER_SEEK_SET 1

static int_32 raw_syscall0(uint_32 number)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number)
                         : "memory");
        return result;
}

static int_32 raw_syscall1(uint_32 number, uint_32 arg)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg)
                         : "memory");
        return result;
}

static int_32 raw_syscall2(uint_32 number, uint_32 arg1, uint_32 arg2)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg1), "c"(arg2)
                         : "memory");
        return result;
}

#ifdef USER_SMOKE_DISK_PREPARE
static int_32 raw_syscall3(uint_32 number, uint_32 arg1, uint_32 arg2,
                           uint_32 arg3)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
                         : "memory");
        return result;
}
#endif

static void report(uint_32 id, bool passed)
{
        raw_syscall2(SYS_TEST_REPORT, id, passed);
}

static void finish(uint_32 value)
{
        raw_syscall1(SYS_TESTSYSCALL, value);
        for (;;)
                __asm__ volatile("pause");
}

static bool run_basic_checks(void)
{
        bool invalid = raw_syscall0(SYS_NR_COUNT + 17) == -ENOSYS;
        bool unimplemented = raw_syscall0(SYS_STAT) == -ENOSYS;

        report(FROG_TEST_SYSCALL_OUT_OF_RANGE, invalid);
        report(FROG_TEST_SYSCALL_UNIMPLEMENTED, unimplemented);
        return invalid && unimplemented;
}

#ifdef USER_SMOKE_PROCESS
static void process_exit(int_32 status) __attribute__((noreturn));

static void process_exit(int_32 status)
{
        raw_syscall1(SYS_EXIT, (uint_32) status);
        for (;;)
                __asm__ volatile("pause");
}

static bool vm_refs_are(uint_32 expected)
{
        return raw_syscall1(SYS_TESTSYSCALL,
                            FROG_TEST_VM_VERIFY_REFS_BASE + expected) == 0;
}

static bool wait_for_status(int_32 pid, int_32 expected)
{
        int_32 status = 0;

        return pid > 0 &&
               raw_syscall1(SYS_WAIT, (uint_32) &status) == pid &&
               status == expected;
}

static bool test_vm_fork_rollback(void)
{
        bool passed = true;

        for (uint_32 step = 0; step < FROG_TEST_VM_FORK_FAIL_COUNT; step++) {
                bool armed = raw_syscall1(
                    SYS_TESTSYSCALL,
                    FROG_TEST_VM_FORK_FAIL_BASE + step) == 0;
                int_32 child = raw_syscall0(SYS_FORK);

                if (child == 0)
                        process_exit(70);
                if (child > 0)
                        (void) wait_for_status(child, 70);
                passed = armed && child == -1 && vm_refs_are(1) && passed;
        }
        return passed;
}

static bool test_vm_parent_first(volatile uint_8 *alias)
{
        const uint_8 waiting = 0x41U;
        const uint_8 release = 0x42U;
        const uint_8 done = 0x43U;
        int_32 worker;

        alias[0] = waiting;
        worker = raw_syscall0(SYS_FORK);
        if (worker == 0) {
                int_32 grandchild = raw_syscall0(SYS_FORK);

                if (grandchild == 0) {
                        while (alias[0] != release)
                                __asm__ volatile("pause");
                        bool passed = vm_refs_are(2);

                        alias[0] = done;
                        process_exit(passed ? 52 : 53);
                }
                process_exit(grandchild > 0 ? 51 : 54);
        }

        bool worker_exit = wait_for_status(worker, 51) && vm_refs_are(2);
        alias[0] = release;
        int_32 status = 0;
        int_32 reparented = raw_syscall1(SYS_WAIT, (uint_32) &status);
        bool grandchild_exit = reparented > 0 && status == 52 &&
                               alias[0] == done && vm_refs_are(1);

        return worker > 0 && worker_exit && grandchild_exit;
}

static void run_vm_lifecycle_checks(volatile uint_8 *alias)
{
        const uint_8 parent_value = 0x31U;
        const uint_8 child_value = 0x32U;
        int_32 child;

        report(FROG_TEST_VM_FORK_ROLLBACK, test_vm_fork_rollback());

        alias[0] = parent_value;
        child = raw_syscall0(SYS_FORK);
        if (child == 0) {
                bool passed = vm_refs_are(2) && alias[0] == parent_value;

                alias[0] = child_value;
                process_exit(passed ? 37 : 38);
        }
        bool shared = wait_for_status(child, 37) &&
                      alias[0] == child_value;
        bool child_first = vm_refs_are(1);
        report(FROG_TEST_VM_FORK_SHARED, shared);
        report(FROG_TEST_VM_EXIT_CHILD_FIRST,
               child > 0 && child_first);

        report(FROG_TEST_VM_EXIT_PARENT_FIRST,
               test_vm_parent_first(alias));

        alias[0] = FROG_TEST_VM_WRITTEN;
        child = raw_syscall0(SYS_FORK);
        if (child == 0) {
                if (raw_syscall1(SYS_TESTSYSCALL,
                                 FROG_TEST_VM_UNMAP_CURRENT) != 0)
                        process_exit(39);
                alias[0] = 0xffU;
                process_exit(40);
        }
        bool post_unmap = wait_for_status(child, -EFAULT) &&
                          vm_refs_are(1) &&
                          alias[0] == FROG_TEST_VM_WRITTEN;
        report(FROG_TEST_VM_POST_UNMAP_FAULT, post_unmap);
}

static bool run_vm_mapping_checks(void)
{
        int_32 mapped = raw_syscall1(SYS_TESTSYSCALL,
                                     FROG_TEST_VM_PREPARE);
        bool address_ok = mapped == (int_32) FROG_TEST_VM_EXPECTED_ADDR;
        bool read_ok = false;
        bool write_ok = false;

        if (address_ok) {
                volatile uint_8 *alias = (volatile uint_8 *) mapped;

                read_ok = alias[0] == FROG_TEST_VM_SEED;
                alias[0] = FROG_TEST_VM_WRITTEN;
                write_ok = alias[0] == FROG_TEST_VM_WRITTEN;
        }
        report(FROG_TEST_VM_USER_ADDRESS, address_ok);
        report(FROG_TEST_VM_USER_READ, read_ok);
        report(FROG_TEST_VM_USER_WRITE, write_ok);

        if (address_ok && read_ok && write_ok)
                run_vm_lifecycle_checks((volatile uint_8 *) mapped);
        else {
                report(FROG_TEST_VM_FORK_SHARED, false);
                report(FROG_TEST_VM_EXIT_CHILD_FIRST, false);
                report(FROG_TEST_VM_FORK_ROLLBACK, false);
                report(FROG_TEST_VM_EXIT_PARENT_FIRST, false);
                report(FROG_TEST_VM_POST_UNMAP_FAULT, false);
        }

        bool cleanup_ok =
            raw_syscall1(SYS_TESTSYSCALL,
                         FROG_TEST_VM_VERIFY_CLEANUP) == 0;
        report(FROG_TEST_VM_CLEANUP, cleanup_ok);
        return address_ok && read_ok && write_ok && cleanup_ok;
}

static void run_profile(void)
{
        volatile int private_value = 7;
        (void) run_basic_checks();
        uint_32 child_pid = raw_syscall0(SYS_FORK);

        if (child_pid == 0) {
                private_value = 19;
                raw_syscall1(SYS_EXIT, 37);
                for (;;)
                        __asm__ volatile("pause");
        }

        report(FROG_TEST_PROCESS_FORK_PARENT_RESULT,
               child_pid != (uint_32) -1 && child_pid != 0);
        if (child_pid != (uint_32) -1) {
                int_32 status = -1;
                int_32 waited_pid =
                    raw_syscall1(SYS_WAIT, (uint_32) &status);

                report(FROG_TEST_PROCESS_WAIT_PID,
                       waited_pid == (int_32) child_pid);
                report(FROG_TEST_PROCESS_WAIT_STATUS, status == 37);
                report(FROG_TEST_PROCESS_FORK_ADDRESS_SPACE,
                       private_value == 7);
        } else {
                report(FROG_TEST_PROCESS_WAIT_PID, false);
                report(FROG_TEST_PROCESS_WAIT_STATUS, false);
                report(FROG_TEST_PROCESS_FORK_ADDRESS_SPACE, false);
        }
        report(FROG_TEST_PROCESS_WAIT_NO_CHILD,
               raw_syscall1(SYS_WAIT, 0) == -1);
        (void) run_vm_mapping_checks();
        finish(1);
}
#elif defined(USER_SMOKE_DISK_PREPARE)
static bool wait_for_child(uint_32 expected_pid)
{
        int_32 status = -1;
        int_32 waited = raw_syscall1(SYS_WAIT, (uint_32) &status);

        return waited == (int_32) expected_pid && status == 0;
}

static bool test_child_close_parent_uses_fd(void)
{
        static const char path[] = "/test/forka";
        static const char value[] = "A";
        int_32 fd = raw_syscall2(SYS_OPEN, (uint_32) path,
                                 O_CREAT | O_EXCL | O_RDWR);
        if (fd < 0)
                return false;
        bool passed = fd == 0 &&
                      raw_syscall3(SYS_WRITE, fd, (uint_32) value, 1) == 1 &&
                      raw_syscall3(SYS_SEEK, fd, 0, USER_SEEK_SET) == 0;
        uint_32 pid = raw_syscall0(SYS_FORK);

        if (pid == 0) {
                bool child_ok = raw_syscall1(SYS_CLOSE, fd) == 0;
                raw_syscall1(SYS_EXIT, child_ok ? 0 : 1);
                for (;;)
                        __asm__ volatile("pause");
        }
        if (pid == (uint_32) -1) {
                passed = false;
        } else {
                char read_value = 0;
                passed = wait_for_child(pid) && passed;
                passed = raw_syscall3(SYS_READ, fd,
                                      (uint_32) &read_value, 1) == 1 &&
                         read_value == 'A' && passed;
        }
        passed = raw_syscall1(SYS_CLOSE, fd) == 0 && passed;
        return raw_syscall1(SYS_UNLINK, (uint_32) path) == 0 && passed;
}

static bool test_parent_close_child_uses_fd(void)
{
        static const char path[] = "/test/forkb";
        static const char value[] = "B";
        int_32 fd = raw_syscall2(SYS_OPEN, (uint_32) path,
                                 O_CREAT | O_EXCL | O_RDWR);
        if (fd < 0)
                return false;
        bool passed = fd == 0 &&
                      raw_syscall3(SYS_WRITE, fd, (uint_32) value, 1) == 1 &&
                      raw_syscall3(SYS_SEEK, fd, 0, USER_SEEK_SET) == 0;
        uint_32 pid = raw_syscall0(SYS_FORK);

        if (pid == 0) {
                char read_value = 0;
                bool child_ok = raw_syscall3(SYS_READ, fd,
                                             (uint_32) &read_value, 1) == 1 &&
                                read_value == 'B';
                child_ok = raw_syscall1(SYS_CLOSE, fd) == 0 && child_ok;
                raw_syscall1(SYS_EXIT, child_ok ? 0 : 1);
                for (;;)
                        __asm__ volatile("pause");
        }
        passed = raw_syscall1(SYS_CLOSE, fd) == 0 && passed;
        if (pid == (uint_32) -1)
                passed = false;
        else
                passed = wait_for_child(pid) && passed;
        return raw_syscall1(SYS_UNLINK, (uint_32) path) == 0 && passed;
}

static void run_profile(void)
{
        (void) run_basic_checks();
        report(FROG_TEST_FD_FORK_CHILD_CLOSE,
               test_child_close_parent_uses_fd());
        report(FROG_TEST_FD_FORK_PARENT_CLOSE,
               test_parent_close_child_uses_fd());
        finish(1);
}
#else
static void run_profile(void)
{
        finish(run_basic_checks() ? USER_SMOKE_MAGIC : 0);
}
#endif

void _start(void) __attribute__((noreturn, section(".text._start")));

void _start(void)
{
        run_profile();
        for (;;)
                __asm__ volatile("pause");
}
