#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/mman.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

#define PAGE_SIZE       4096U
#define MULTI_LENGTH    (3U * PAGE_SIZE)
#define BACKBUFFER_LENGTH (3U * 1024U * 1024U)
#define MAP_MAX_LENGTH  (16U * 1024U * 1024U)
#define FIRST_MAP       0x40000000U

static bool all_passed = true;

static int_32 raw_syscall1(uint_32 number, uint_32 arg)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg)
                         : "memory");
        return result;
}

static int_32 raw_syscall2(uint_32 number, uint_32 first, uint_32 second)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(first), "c"(second)
                         : "memory");
        return result;
}

static int_32 raw_syscall3(uint_32 number, uint_32 first, uint_32 second,
                           uint_32 third)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(first), "c"(second),
                           "d"(third)
                         : "memory");
        return result;
}

static void report(uint_32 id, bool passed)
{
        if (raw_syscall2(SYS_TEST_REPORT, id, passed) != 0)
                passed = false;
        all_passed = passed && all_passed;
}

static void finish(void) __attribute__((noreturn));
static void finish(void)
{
        raw_syscall1(SYS_TESTSYSCALL, FROG_TEST_ANON_MMAP_FINISH);
        for (;;)
                __asm__ volatile("pause");
}

static void process_exit(int_32 status) __attribute__((noreturn));
static void process_exit(int_32 status)
{
        raw_syscall1(SYS_EXIT, (uint_32) status);
        for (;;)
                __asm__ volatile("pause");
}

static int_32 map_raw(uint_32 length)
{
        struct frog_mmap_args args = {
            .addr = 0,
            .length = length,
            .prot = PROT_READ | PROT_WRITE,
            .flags = MAP_PRIVATE | MAP_ANONYMOUS,
            .fd = -1,
            .offset = 0,
        };

        return raw_syscall1(SYS_MMAP, (uint_32) &args);
}

static bool zeroed(volatile uint_8 *mapping, uint_32 length)
{
        for (uint_32 index = 0; index < length; index++) {
                if (mapping[index] != 0)
                        return false;
        }
        return true;
}

static bool invalid_arguments(void)
{
        struct frog_mmap_args args = {
            .addr = 0,
            .length = PAGE_SIZE,
            .prot = PROT_READ | PROT_WRITE,
            .flags = MAP_PRIVATE | MAP_ANONYMOUS,
            .fd = -1,
            .offset = 0,
        };
        bool passed = raw_syscall1(SYS_MMAP, 0) == -EFAULT &&
            raw_syscall1(SYS_MMAP, 0xbffffff0U) == -EFAULT;

        args.addr = PAGE_SIZE;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.addr = 0;
        args.length = 0;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.length = PAGE_SIZE - 1U;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.length = MAP_MAX_LENGTH + PAGE_SIZE;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.length = PAGE_SIZE;
        args.prot = PROT_READ;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.prot = PROT_READ | PROT_WRITE;
        args.flags = MAP_PRIVATE;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.flags = MAP_PRIVATE | MAP_ANONYMOUS;
        args.fd = 0;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        args.fd = -1;
        args.offset = PAGE_SIZE;
        passed = raw_syscall1(SYS_MMAP, (uint_32) &args) < 0 && passed;
        return passed;
}

static bool map_and_unmap(void)
{
        int_32 first_result = map_raw(PAGE_SIZE);
        volatile uint_8 *first = (volatile uint_8 *) first_result;
        bool one_page = first_result == (int_32) FIRST_MAP &&
            zeroed(first, PAGE_SIZE);

        if (one_page) {
                first[0] = 0x11U;
                first[PAGE_SIZE - 1U] = 0x12U;
                one_page = first[0] == 0x11U &&
                           first[PAGE_SIZE - 1U] == 0x12U;
        }
        report(FROG_TEST_ANON_MMAP_ONE_PAGE, one_page);
        if (!one_page)
                return false;

        int_32 multi_result = map_raw(MULTI_LENGTH);
        volatile uint_8 *multi = (volatile uint_8 *) multi_result;
        bool multi_page = multi_result == (int_32) (FIRST_MAP + PAGE_SIZE) &&
            zeroed(multi, MULTI_LENGTH);

        if (multi_page) {
                for (uint_32 page = 0; page < 3U; page++)
                        multi[page * PAGE_SIZE + page] = (uint_8) (0x21U + page);
                multi_page = multi[0] == 0x21U &&
                    multi[PAGE_SIZE + 1U] == 0x22U &&
                    multi[2U * PAGE_SIZE + 2U] == 0x23U;
        }
        report(FROG_TEST_ANON_MMAP_MULTIPAGE, multi_page);
        if (!multi_page)
                return false;

        bool exact = raw_syscall2(SYS_MUNMAP, multi_result, PAGE_SIZE) < 0 &&
            raw_syscall2(SYS_MUNMAP, multi_result + PAGE_SIZE,
                         2U * PAGE_SIZE) < 0 &&
            raw_syscall2(SYS_MUNMAP, multi_result, MULTI_LENGTH) == 0 &&
            raw_syscall2(SYS_MUNMAP, multi_result, MULTI_LENGTH) < 0 &&
            raw_syscall2(SYS_MUNMAP, first_result, PAGE_SIZE) == 0;
        report(FROG_TEST_ANON_MUNMAP_EXACT, exact);
        return exact;
}

static bool fork_private(void)
{
        int_32 mapped = map_raw(PAGE_SIZE);
        volatile uint_8 *page = (volatile uint_8 *) mapped;
        bool passed = mapped == (int_32) FIRST_MAP;

        if (!passed)
                return false;
        page[0] = 0x31U;
        int_32 child = raw_syscall1(SYS_FORK, 0);
        if (child == 0) {
                bool isolated = raw_syscall3(SYS_WAIT2, 0, 0, 10) == 0 &&
                                page[0] == 0x31U;
                page[0] = 0x53U;
                process_exit(isolated && page[0] == 0x53U ? 71 : 72);
        }
        if (child < 0)
                passed = false;
        page[0] = 0x42U;
        int_32 status = 0;
        passed = raw_syscall1(SYS_WAIT, (uint_32) &status) == child &&
                 status == 71 && page[0] == 0x42U && passed;
        passed = raw_syscall2(SYS_MUNMAP, mapped, PAGE_SIZE) == 0 && passed;
        return passed;
}

static bool exit_cleanup(void)
{
        bool passed = raw_syscall1(SYS_TESTSYSCALL,
            FROG_TEST_ANON_MMAP_SNAPSHOT) == 0;
        int_32 child = raw_syscall1(SYS_FORK, 0);

        if (child == 0) {
                int_32 mapped = map_raw(MULTI_LENGTH);

                if (mapped < 0)
                        process_exit(81);
                ((volatile uint_8 *) mapped)[MULTI_LENGTH - 1U] = 0x6aU;
                process_exit(82);
        }
        int_32 status = 0;
        passed = child > 0 &&
            raw_syscall1(SYS_WAIT, (uint_32) &status) == child &&
            status == 82 && passed;
        passed = raw_syscall1(SYS_TESTSYSCALL,
            FROG_TEST_ANON_MMAP_VERIFY) == 0 && passed;
        return passed;
}

static bool exec_cleanup(void)
{
        static const char target[] = "/test/exec-target";
        static const char input_path[] = "/test/exec-input";
        static const char arg0[] = "exec-target";
        static const char arg1[] = "alpha";
        static const char arg2[] = "beta";
        const char *argv[] = {arg0, arg1, arg2, NULL};
        int_32 input_fd = raw_syscall2(
            SYS_OPEN, (uint_32) input_path, O_RDONLY);
        bool input_ready = input_fd == 0;
        bool snapshot = raw_syscall1(
            SYS_TESTSYSCALL, FROG_TEST_ANON_MMAP_SNAPSHOT) == 0;
        int_32 child = input_ready && snapshot ?
            raw_syscall1(SYS_FORK, 0) : -1;

        if (child == 0) {
                int_32 mapped = map_raw(MULTI_LENGTH);

                if (mapped < 0)
                        process_exit(83);
                volatile uint_8 *anonymous = (volatile uint_8 *) mapped;
                anonymous[0] = 0x4dU;
                anonymous[PAGE_SIZE] = 0x5aU;
                anonymous[MULTI_LENGTH - 1U] = 0xa5U;
                (void) raw_syscall2(SYS_EXECV, (uint_32) target,
                                    (uint_32) argv);
                process_exit(84);
        }

        int_32 status = 0;
        bool executed = child > 0 &&
            raw_syscall1(SYS_WAIT, (uint_32) &status) == child &&
            status == FROG_TEST_EXEC_TARGET_STATUS;
        bool frames_released = raw_syscall1(
            SYS_TESTSYSCALL, FROG_TEST_ANON_MMAP_VERIFY) == 0;
        bool cleaned = input_fd >= 0 &&
            raw_syscall1(SYS_CLOSE, input_fd) == 0;

        return input_ready && snapshot && executed && frames_released &&
               cleaned;
}

static bool rollback(void)
{
        bool passed = true;

        for (uint_32 step = 0; step < FROG_TEST_ANON_MMAP_FAIL_COUNT; step++) {
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_MMAP_SNAPSHOT) == 0 && passed;
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_MMAP_FAIL_BASE + step) == 0 && passed;
                passed = map_raw(MULTI_LENGTH) == -ENOMEM && passed;
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_MMAP_VERIFY) == 0 && passed;
        }

        int_32 mapped = map_raw(MULTI_LENGTH);
        passed = mapped == (int_32) FIRST_MAP &&
                 zeroed((volatile uint_8 *) mapped, MULTI_LENGTH) && passed;
        if (mapped >= 0)
                passed = raw_syscall2(SYS_MUNMAP, mapped,
                                      MULTI_LENGTH) == 0 && passed;
        return passed;
}

static bool fork_rollback(void)
{
        int_32 mapped = map_raw(PAGE_SIZE);
        volatile uint_8 *page = (volatile uint_8 *) mapped;
        bool passed = mapped == (int_32) FIRST_MAP;

        if (!passed)
                return false;
        page[0] = 0x7bU;
        for (uint_32 step = 0; step < FROG_TEST_ANON_FORK_FAIL_COUNT;
             step++) {
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_MMAP_SNAPSHOT) == 0 && passed;
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_FORK_FAIL_BASE + step) == 0 && passed;
                passed = raw_syscall1(SYS_FORK, 0) == -1 && passed;
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_MMAP_VERIFY) == 0 && passed;
                passed = page[0] == 0x7bU && passed;
        }
        passed = raw_syscall2(SYS_MUNMAP, mapped, PAGE_SIZE) == 0 && passed;
        return passed;
}

static bool allocation_rollback(void)
{
        bool passed = true;

        for (uint_32 step = 0; step < FROG_TEST_ANON_ALLOC_FAIL_COUNT;
             step++) {
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_MMAP_SNAPSHOT) == 0 && passed;
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_ALLOC_FAIL_BASE + step) == 0 && passed;
                passed = map_raw(MULTI_LENGTH) == -ENOMEM && passed;
                passed = raw_syscall1(SYS_TESTSYSCALL,
                    FROG_TEST_ANON_MMAP_VERIFY) == 0 && passed;
        }
        int_32 mapped = map_raw(MULTI_LENGTH);

        passed = mapped == (int_32) FIRST_MAP && passed;
        if (mapped >= 0)
                passed = raw_syscall2(SYS_MUNMAP, mapped,
                                      MULTI_LENGTH) == 0 && passed;
        return passed;
}

static bool maximum_length(void)
{
        int_32 mapped = map_raw(MAP_MAX_LENGTH);
        volatile uint_8 *bytes = (volatile uint_8 *) mapped;

#ifdef FROG_TEST_ALLOW_MAX_OOM
        if (mapped == -ENOMEM)
                return true;
#endif
        bool passed = mapped == (int_32) FIRST_MAP &&
            zeroed(bytes, MAP_MAX_LENGTH);

        if (passed) {
                bytes[0] = 0x91U;
                bytes[MAP_MAX_LENGTH - 1U] = 0x92U;
                passed = bytes[0] == 0x91U &&
                         bytes[MAP_MAX_LENGTH - 1U] == 0x92U;
        }
        if (mapped >= 0)
                passed = raw_syscall2(SYS_MUNMAP, mapped,
                                      MAP_MAX_LENGTH) == 0 && passed;
        return passed;
}

static bool backbuffer_budget(void)
{
        bool passed = raw_syscall1(SYS_TESTSYSCALL,
            FROG_TEST_ANON_MMAP_SNAPSHOT) == 0;
        int_32 mapped = map_raw(BACKBUFFER_LENGTH);
        volatile uint_8 *bytes = (volatile uint_8 *) mapped;

        passed = mapped == (int_32) FIRST_MAP &&
                 zeroed(bytes, BACKBUFFER_LENGTH) && passed;
        if (mapped >= 0) {
                volatile uint_32 *cross_page =
                    (volatile uint_32 *) (bytes + PAGE_SIZE - 2U);

                bytes[0] = 0xa1U;
                *cross_page = 0xb2c3d4e5U;
                bytes[BACKBUFFER_LENGTH - 1U] = 0xf6U;
                passed = bytes[0] == 0xa1U &&
                         *cross_page == 0xb2c3d4e5U &&
                         bytes[BACKBUFFER_LENGTH - 1U] == 0xf6U && passed;
                passed = raw_syscall2(SYS_MUNMAP, mapped,
                                      BACKBUFFER_LENGTH) == 0 && passed;
        }
        passed = raw_syscall1(SYS_TESTSYSCALL,
            FROG_TEST_ANON_MMAP_VERIFY) == 0 && passed;
        return passed;
}

void _start(void) __attribute__((section(".text._start"), noreturn));
void _start(void)
{
        int_32 probe = map_raw(PAGE_SIZE);
        bool supported = probe == (int_32) FIRST_MAP;

        report(FROG_TEST_ANON_MMAP_SUPPORTED, supported);
        if (!supported)
                finish();
        supported = zeroed((volatile uint_8 *) probe, PAGE_SIZE) &&
            raw_syscall2(SYS_MUNMAP, probe, PAGE_SIZE) == 0;
        if (!supported) {
                report(FROG_TEST_ANON_MMAP_SUPPORTED, false);
                finish();
        }

        report(FROG_TEST_ANON_MMAP_ARGUMENTS, invalid_arguments());
        if (!map_and_unmap())
                finish();
        report(FROG_TEST_ANON_MMAP_FORK_PRIVATE, fork_private());
        report(FROG_TEST_ANON_MMAP_EXIT_CLEANUP, exit_cleanup());
        report(FROG_TEST_ANON_MMAP_EXEC_CLEANUP, exec_cleanup());
        report(FROG_TEST_ANON_MMAP_ROLLBACK, rollback());
        report(FROG_TEST_ANON_MMAP_FORK_ROLLBACK, fork_rollback());
        report(FROG_TEST_ANON_MMAP_ALLOC_ROLLBACK,
               allocation_rollback());
        report(FROG_TEST_ANON_MMAP_BACKBUFFER, backbuffer_budget());
        report(FROG_TEST_ANON_MMAP_MAX_LENGTH, maximum_length());
        (void) all_passed;
        finish();
}
