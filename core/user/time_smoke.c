#include <frog/errno.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/time.h>
#include <frog/types.h>

#define TIME_SMOKE_CLOCK_GETTIME_NR SYS_CLOCK_GETTIME
#define TIME_SMOKE_CLOCK_REALTIME   CLOCK_REALTIME
#define TIME_SMOKE_CLOCK_MONOTONIC  CLOCK_MONOTONIC
#define TIME_SMOKE_NSEC_PER_SEC     1000000000
#define TIME_SMOKE_USER_LIMIT       0xc0000000U

typedef char time_smoke_timespec_must_be_12_bytes[
    sizeof(struct timespec) == 12 ? 1 : -1];
typedef char time_smoke_timeval_must_be_12_bytes[
    sizeof(struct timeval) == 12 ? 1 : -1];

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

static void report(uint_32 id, bool passed)
{
        raw_syscall2(SYS_TEST_REPORT, id, passed);
}

static void finish(void)
{
        raw_syscall1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}

static bool timespec_normalized(const struct timespec *value)
{
        return value->tv_sec >= 0 && value->tv_nsec >= 0 &&
               value->tv_nsec < TIME_SMOKE_NSEC_PER_SEC;
}

static int timespec_compare(const struct timespec *left,
                            const struct timespec *right)
{
        if (left->tv_sec != right->tv_sec)
                return left->tv_sec < right->tv_sec ? -1 : 1;
        if (left->tv_nsec != right->tv_nsec)
                return left->tv_nsec < right->tv_nsec ? -1 : 1;
        return 0;
}

static bool monotonic_eventually_advances(void)
{
        struct timespec previous;
        struct timespec current;

        if (raw_syscall2(TIME_SMOKE_CLOCK_GETTIME_NR,
                         TIME_SMOKE_CLOCK_MONOTONIC,
                         (uint_32) &previous) != 0 ||
            !timespec_normalized(&previous))
                return false;

        for (uint_32 attempt = 0; attempt < 100000U; attempt++) {
                int ordering;

                if (raw_syscall2(TIME_SMOKE_CLOCK_GETTIME_NR,
                                 TIME_SMOKE_CLOCK_MONOTONIC,
                                 (uint_32) &current) != 0 ||
                    !timespec_normalized(&current))
                        return false;
                ordering = timespec_compare(&current, &previous);
                if (ordering < 0)
                        return false;
                if (ordering > 0)
                        return true;
                previous = current;
        }
        return false;
}

void _start(void)
{
        struct timespec now;
        struct timeval realtime;
        int_32 result;

        now.tv_sec = -1;
        now.tv_nsec = -1;
        result = raw_syscall2(TIME_SMOKE_CLOCK_GETTIME_NR,
                              TIME_SMOKE_CLOCK_MONOTONIC,
                              (uint_32) &now);
        report(FROG_TEST_TIME_MONOTONIC_NORMALIZED,
               result == 0 && timespec_normalized(&now));
        report(FROG_TEST_TIME_MONOTONIC_ADVANCES,
               monotonic_eventually_advances());
        report(FROG_TEST_TIME_MONOTONIC_BAD_POINTERS,
               raw_syscall2(TIME_SMOKE_CLOCK_GETTIME_NR,
                            TIME_SMOKE_CLOCK_MONOTONIC, 0) == -EFAULT &&
               raw_syscall2(TIME_SMOKE_CLOCK_GETTIME_NR,
                            TIME_SMOKE_CLOCK_MONOTONIC,
                            TIME_SMOKE_USER_LIMIT - 8U) == -EFAULT);
        report(FROG_TEST_TIME_INVALID_CLOCK,
               raw_syscall2(TIME_SMOKE_CLOCK_GETTIME_NR, 2U,
                            (uint_32) &now) == -EINVAL);
        report(FROG_TEST_TIME_REALTIME_UNAVAILABLE,
               raw_syscall2(TIME_SMOKE_CLOCK_GETTIME_NR,
                            TIME_SMOKE_CLOCK_REALTIME,
                            (uint_32) &now) == -ENODATA &&
               raw_syscall2(SYS_GETTIMEOFDAY, (uint_32) &realtime, 0) ==
                   -ENODATA);
        report(FROG_TEST_TIME_SETTIMEOFDAY_UNIMPLEMENTED,
               raw_syscall2(SYS_SETTIMEOFDAY, 0, 0) == -ENOSYS);
        finish();
}
