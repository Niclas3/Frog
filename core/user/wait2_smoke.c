#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/time.h>
#include <frog/types.h>

#define WAIT2_MAX_FDS    32U
typedef char wait2_pollfd_must_be_8_bytes[
    sizeof(struct pollfd) == 8 ? 1 : -1];

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

static int_32 raw_syscall3(uint_32 number, uint_32 arg1,
                           uint_32 arg2, uint_32 arg3)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3)
                         : "memory");
        return result;
}

static int_32 call_wait2(struct pollfd *fds, uint_32 count, int_32 timeout_ms)
{
        return raw_syscall3(SYS_WAIT2, (uint_32) fds, count,
                            (uint_32) timeout_ms);
}

static void report(uint_32 id, bool passed)
{
        (void) raw_syscall2(SYS_TEST_REPORT, id, passed);
}

static void finish(void)
{
        (void) raw_syscall1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
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

static void deadline_after_ms(struct timespec *deadline,
                              const struct timespec *start,
                              uint_32 timeout_ms)
{
        deadline->tv_sec = start->tv_sec + timeout_ms / 1000U;
        deadline->tv_nsec = start->tv_nsec +
                            (int_32) ((timeout_ms % 1000U) * 1000000U);
        if (deadline->tv_nsec >= 1000000000) {
                deadline->tv_sec++;
                deadline->tv_nsec -= 1000000000;
        }
}

static bool finite_sleep_not_early(uint_32 timeout_ms)
{
        struct timespec start;
        struct timespec end;
        struct timespec deadline;

        if (raw_syscall2(SYS_CLOCK_GETTIME, CLOCK_MONOTONIC,
                         (uint_32) &start) != 0 ||
            call_wait2(0, 0, (int_32) timeout_ms) != 0 ||
            raw_syscall2(SYS_CLOCK_GETTIME, CLOCK_MONOTONIC,
                         (uint_32) &end) != 0)
                return false;
        deadline_after_ms(&deadline, &start, timeout_ms);
        return timespec_compare(&end, &deadline) >= 0;
}

static bool check_argument_validation(void)
{
        struct pollfd unsupported = { .fd = -1, .events = 0x0002,
                                      .revents = 0xffff };

        return call_wait2(0, 0, -2) == -EINVAL &&
               call_wait2(0, 0, -1) == -EINVAL &&
               call_wait2(0, WAIT2_MAX_FDS + 1U, 0) == -EINVAL &&
               call_wait2(0, 1, 0) == -EFAULT &&
               call_wait2(&unsupported, 1, 0) == -EINVAL;
}

static bool check_negative_fd(void)
{
        struct pollfd descriptor = { .fd = -1, .events = POLLIN,
                                     .revents = 0xffff };

        return call_wait2(&descriptor, 1, 0) == 0 &&
               descriptor.revents == 0;
}

static bool check_invalid_fd(void)
{
        struct pollfd descriptor = { .fd = 123, .events = POLLIN,
                                     .revents = 0xffff };

        return call_wait2(&descriptor, 1, 0) == 1 &&
               descriptor.revents == POLLNVAL;
}

static bool check_revents_reset(void)
{
        struct pollfd descriptor = { .fd = 123, .events = POLLIN,
                                     .revents = 0xffff };

        if (call_wait2(&descriptor, 1, 0) != 1 ||
            descriptor.revents != POLLNVAL)
                return false;
        descriptor.fd = -1;
        return call_wait2(&descriptor, 1, 0) == 0 &&
               descriptor.revents == 0;
}

static bool check_timeout_zero(void)
{
        struct pollfd descriptor = { .fd = 123, .events = POLLIN,
                                     .revents = 0xffff };

        return call_wait2(&descriptor, 1, 0) == 1 &&
               descriptor.revents == POLLNVAL;
}

static bool check_multiple_invalid(void)
{
        struct pollfd descriptors[3] = {
            { .fd = 111, .events = POLLIN, .revents = 0xffff },
            { .fd = -1, .events = POLLOUT, .revents = 0xffff },
            { .fd = 112, .events = POLLOUT, .revents = 0xffff },
        };

        return call_wait2(descriptors, 3, 0) == 2 &&
               descriptors[0].revents == POLLNVAL &&
               descriptors[1].revents == 0 &&
               descriptors[2].revents == POLLNVAL;
}

static bool check_copyout_cleanup(void)
{
        static const char keyboard_path[] = "/dev/input/event0";
        struct pollfd descriptor;
        int_32 fd = raw_syscall2(SYS_OPEN, (uint_32) keyboard_path,
                                 O_RDONLY | O_NONBLOCK);
        bool passed;

        if (fd < 0)
                return false;
        descriptor.fd = fd;
        descriptor.events = POLLIN;
        descriptor.revents = 0xffff;
        passed = raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_WAIT2_ARM_COPYOUT_FAULT) == 0 &&
                 call_wait2(&descriptor, 1, 1) == -EFAULT &&
                 raw_syscall1(SYS_TEST_SYNC,
                              FROG_TEST_WAIT2_VERIFY_CLEANUP) == 0 &&
                 raw_syscall1(SYS_CLOSE, fd) == 0;
        if (!passed)
                (void) raw_syscall1(SYS_CLOSE, fd);
        return passed;
}

void _start(void)
{
        report(FROG_TEST_WAIT2_AVAILABLE, call_wait2(0, 0, 0) == 0);
        report(FROG_TEST_WAIT2_ABI, sizeof(struct pollfd) == 8);
        report(FROG_TEST_WAIT2_ARGUMENTS, check_argument_validation());
        report(FROG_TEST_WAIT2_NEGATIVE_FD, check_negative_fd());
        report(FROG_TEST_WAIT2_INVALID_FD, check_invalid_fd());
        report(FROG_TEST_WAIT2_REVENTS_RESET, check_revents_reset());
        report(FROG_TEST_WAIT2_TIMEOUT_ZERO, check_timeout_zero());
        report(FROG_TEST_WAIT2_FINITE_SLEEP, finite_sleep_not_early(1));
        report(FROG_TEST_WAIT2_BOUNDARY_SLEEP,
               finite_sleep_not_early(1001));
        report(FROG_TEST_WAIT2_MULTIPLE_INVALID, check_multiple_invalid());
        report(FROG_TEST_WAIT2_COPYOUT_CLEANUP, check_copyout_cleanup());
        finish();
}
