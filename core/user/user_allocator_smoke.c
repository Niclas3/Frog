#include <frog/errno.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

#define PAGE_SIZE       4096U
#define LARGE_SIZE      (3U * 1024U * 1024U)
#define CHILD_ROUNDS    8U

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

static void report(uint_32 id, bool passed)
{
        (void) raw_syscall2(SYS_TEST_REPORT, id, passed);
}

static void finish(void) __attribute__((noreturn));
static void finish(void)
{
        (void) raw_syscall1(SYS_TESTSYSCALL,
                            FROG_TEST_USER_ALLOC_FINISH);
        for (;;)
                __asm__ volatile("pause");
}

static void child_exit(int_32 status) __attribute__((noreturn));
static void child_exit(int_32 status)
{
        (void) raw_syscall1(SYS_EXIT, (uint_32) status);
        for (;;)
                __asm__ volatile("pause");
}

static bool snapshot(void)
{
        return raw_syscall1(SYS_TESTSYSCALL,
                            FROG_TEST_USER_ALLOC_SNAPSHOT) == 0;
}

static bool verify(void)
{
        return raw_syscall1(SYS_TESTSYSCALL,
                            FROG_TEST_USER_ALLOC_VERIFY) == 0;
}

static bool alignment_and_writes(void)
{
        static const uint_32 sizes[] = {
            1U, 15U, 16U, 17U, 63U, 255U, 513U, 1024U, 1025U,
        };
        void *blocks[sizeof(sizes) / sizeof(sizes[0])] = {NULL};
        bool passed = true;

        for (uint_32 index = 0; index < sizeof(sizes) / sizeof(sizes[0]);
             index++) {
                blocks[index] = malloc(sizes[index]);
                uint_8 *bytes = blocks[index];

                if (bytes == NULL || ((uint_32) bytes & 15U) != 0) {
                        passed = false;
                        break;
                }
                bytes[0] = (uint_8) (0x20U + index);
                bytes[sizes[index] - 1U] = (uint_8) (0x70U + index);
                passed = (sizes[index] == 1U ||
                          bytes[0] == (uint_8) (0x20U + index)) &&
                         bytes[sizes[index] - 1U] ==
                             (uint_8) (0x70U + index) &&
                         passed;
        }
        for (uint_32 index = 0; index < sizeof(sizes) / sizeof(sizes[0]);
             index++)
                free(blocks[index]);
        return passed;
}

static bool small_reuse(void)
{
        uint_8 *first = malloc(33);
        uint_8 *keeper = malloc(33);

        if (first == NULL || keeper == NULL) {
                free(first);
                free(keeper);
                return false;
        }
        keeper[0] = 0x5aU;
        free(first);
        uint_8 *reused = malloc(33);
        bool passed = reused == first && keeper[0] == 0x5aU;

        free(reused);
        free(keeper);
        return passed;
}

static bool arena_release(void)
{
        bool passed = snapshot();
        void *small = malloc(16);
        void *medium = malloc(200);
        void *large_small = malloc(1024);

        passed = small != NULL && medium != NULL &&
                 large_small != NULL && passed;
        free(small);
        free(medium);
        free(large_small);
        return verify() && passed;
}

static bool large_release(void)
{
        bool passed = snapshot();
        uint_8 *bytes = malloc(LARGE_SIZE);

        passed = bytes != NULL && ((uint_32) bytes & 15U) == 0 && passed;
        if (bytes != NULL) {
                for (uint_32 index = 0; index < LARGE_SIZE; index++) {
                        if (bytes[index] != 0) {
                                passed = false;
                                break;
                        }
                }
                bytes[0] = 0x31U;
                bytes[PAGE_SIZE - 1U] = 0x42U;
                bytes[LARGE_SIZE - 1U] = 0x53U;
                passed = bytes[0] == 0x31U &&
                    bytes[PAGE_SIZE - 1U] == 0x42U &&
                    bytes[LARGE_SIZE - 1U] == 0x53U && passed;
        }
        free(bytes);
        return verify() && passed;
}

static bool forced_oom(void)
{
        bool passed = snapshot() &&
            raw_syscall1(SYS_TESTSYSCALL,
                         FROG_TEST_USER_ALLOC_FAIL_NEXT) == 0;
        void *failed = malloc(LARGE_SIZE);

        passed = failed == NULL && verify() && passed;
        void *retry = malloc(64);
        passed = retry != NULL && passed;
        free(retry);
        return passed;
}

static bool fork_isolation(void)
{
        bool passed = snapshot();
        uint_8 *parent = malloc(64);

        if (parent == NULL)
                return verify() && false;
        parent[0] = 0x61U;
        pid_t child = fork();
        if (child == 0) {
                uint_8 *own = malloc(64);
                bool child_ok = parent[0] == 0x61U && own != NULL;

                parent[0] = 0x72U;
                if (own != NULL)
                        own[0] = 0x83U;
                child_ok = parent[0] == 0x72U &&
                    (own == NULL || own[0] == 0x83U) && child_ok;
                free(own);
                free(parent);
                child_exit(child_ok ? 91 : 92);
        }
        int_32 status = 0;
        passed = child > 0 && wait(&status) == child && status == 91 &&
                 parent[0] == 0x61U && passed;
        free(parent);
        return verify() && passed;
}

static bool child_exit_cleanup(void)
{
        bool passed = snapshot();

        for (uint_32 round = 0; round < CHILD_ROUNDS; round++) {
                pid_t child = fork();
                if (child == 0) {
                        uint_8 *small = malloc(128);
                        uint_8 *large = malloc(1024U * 1024U);

                        if (small != NULL)
                                small[0] = (uint_8) round;
                        if (large != NULL)
                                large[1024U * 1024U - 1U] = (uint_8) round;
                        child_exit(small != NULL && large != NULL ? 93 : 94);
                }
                int_32 status = 0;
                passed = child > 0 && wait(&status) == child &&
                         status == 93 && passed;
        }
        return verify() && passed;
}

void _start(void) __attribute__((section(".text._start"), noreturn));
void _start(void)
{
        bool legacy = raw_syscall1(SYS_MALLOC, 16) == -ENOSYS &&
                      raw_syscall1(SYS_FREE, 0) == -ENOSYS;
        report(FROG_TEST_USER_ALLOC_LEGACY_SYSCALLS, legacy);

        bool zero_null = malloc(0) == NULL;
        free(NULL);
        report(FROG_TEST_USER_ALLOC_ZERO_NULL, zero_null);
        if (!zero_null)
                finish();

        report(FROG_TEST_USER_ALLOC_LIMITS,
               malloc(0xffffffffU) == NULL &&
               malloc(16U * 1024U * 1024U) == NULL);
        report(FROG_TEST_USER_ALLOC_ALIGNMENT_WRITE,
               alignment_and_writes());
        report(FROG_TEST_USER_ALLOC_SMALL_REUSE, small_reuse());
        report(FROG_TEST_USER_ALLOC_ARENA_RELEASE, arena_release());
        report(FROG_TEST_USER_ALLOC_LARGE_RELEASE, large_release());
        report(FROG_TEST_USER_ALLOC_FORCED_OOM, forced_oom());
        report(FROG_TEST_USER_ALLOC_FORK_ISOLATION, fork_isolation());
        report(FROG_TEST_USER_ALLOC_EXIT_CLEANUP,
               child_exit_cleanup());
        finish();
}
