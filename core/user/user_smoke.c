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

void _start(void) __attribute__((noreturn));

void _start(void)
{
        run_profile();
        for (;;)
                __asm__ volatile("pause");
}
