#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

static void finish(void) __attribute__((noreturn));

static void finish(void)
{
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}

void _start(void) __attribute__((section(".text._start"), noreturn));

void _start(void)
{
        static const char path[] = "/test/poudland-e2e";
        static const char *argv[] = {path, NULL};
        int_32 result;

        (void) execv(path, argv);
        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT),
                           "b"(FROG_TEST_POUDLAND_E2E_HARNESS_EXEC),
                           "c"(false)
                         : "memory");
        finish();
}
