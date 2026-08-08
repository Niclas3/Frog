#include "test.h"

#include <frog/syscall.h>

static int_32 raw_syscall1(uint_32 number, uint_32 arg)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(arg)
                         : "memory");
        return result;
}

#ifdef FROG_POUDLAND_P0_TEST
static int_32 raw_syscall2(uint_32 number, uint_32 first, uint_32 second)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(first), "c"(second)
                         : "memory");
        return result;
}
#endif

void poudland_p0_test_report(uint_32 id, bool passed)
{
#ifdef FROG_POUDLAND_P0_TEST
        (void) raw_syscall2(SYS_TEST_REPORT, id, passed);
#else
        (void) id;
        (void) passed;
#endif
}

int_32 poudland_p0_test_sync(uint_32 command)
{
#ifdef FROG_POUDLAND_P0_TEST
        return raw_syscall1(SYS_TEST_SYNC, command);
#else
        (void) command;
        return 0;
#endif
}

void poudland_p0_test_finish(void)
{
#ifdef FROG_POUDLAND_P0_TEST
        (void) raw_syscall1(SYS_TESTSYSCALL, 0);
#else
        (void) raw_syscall1(SYS_EXIT, 1);
#endif
        for (;;)
                __asm__ volatile("pause");
}
