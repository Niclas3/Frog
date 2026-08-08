#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

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

void _start(void) __attribute__((section(".text._start"), noreturn));
void _start(void)
{
        static const char path[] = "/test/compositor";
        static const char *argv[] = {path, NULL};

        (void) execv(path, argv);
        (void) raw_syscall2(SYS_TEST_REPORT,
                            FROG_TEST_POUDLAND_BUILTIN_EXEC, false);
        (void) raw_syscall1(SYS_TESTSYSCALL, 0);
        for (;;)
                __asm__ volatile("pause");
}
