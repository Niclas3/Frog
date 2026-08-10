#include <frog/fcntl.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

#define FROGFS_EXEC_SEEK_END 3U
#define FROGFS_EXEC_ONE_PAGE 4096
#define FROGFS_EXEC_FAILURE_STATUS 97

static int_32 call0(uint_32 number)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number)
                         : "memory");
        return result;
}

static int_32 call1(uint_32 number, uint_32 arg)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg)
                         : "memory");
        return result;
}

static int_32 call2(uint_32 number, uint_32 arg1, uint_32 arg2)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg1), "c"(arg2)
                         : "memory");
        return result;
}

static int_32 call3(uint_32 number, uint_32 arg1, uint_32 arg2,
                    uint_32 arg3)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
                         : "memory");
        return result;
}

static void report(uint_32 id, bool passed)
{
        (void) call2(SYS_TEST_REPORT, id, passed);
}

static bool file_is_multipage(const char *path)
{
        int_32 fd = call2(SYS_OPEN, (uint_32) path, O_RDONLY);
        int_32 size;
        bool closed;

        if (fd < 0)
                return false;
        size = call3(SYS_SEEK, fd, 0, FROGFS_EXEC_SEEK_END);
        closed = call1(SYS_CLOSE, fd) == 0;
        return size > FROGFS_EXEC_ONE_PAGE && closed;
}

static bool exec_and_wait(const char *path, int_32 expected_status)
{
        const char *argv[] = { path, "--exec-smoke", NULL };
        int_32 child = call0(SYS_FORK);
        int_32 status = -1;

        if (child == 0) {
                (void) call2(SYS_EXECV, (uint_32) path, (uint_32) argv);
                (void) call1(SYS_EXIT, FROGFS_EXEC_FAILURE_STATUS);
                for (;;)
                        __asm__ volatile("pause");
        }
        return child > 0 && call1(SYS_WAIT, (uint_32) &status) == child &&
               status == expected_status;
}

void _start(void) __attribute__((section(".text._start"), noreturn));

void _start(void)
{
        static const char compositor[] = "/test/bin/compositor";
        static const char desktop[] = "/test/bin/desktop";
        bool passed;

        passed = file_is_multipage(compositor);
        report(FROG_TEST_FROGFS_EXEC_COMPOSITOR_SIZE, passed);
        if (passed)
                passed = exec_and_wait(compositor, 42);
        report(FROG_TEST_FROGFS_EXEC_COMPOSITOR, passed);

        passed = file_is_multipage(desktop);
        report(FROG_TEST_FROGFS_EXEC_DESKTOP_SIZE, passed);
        if (passed)
                passed = exec_and_wait(desktop, 43);
        report(FROG_TEST_FROGFS_EXEC_DESKTOP, passed);

        (void) call1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}
