#include <frog/errno.h>
#include <frog/syscall.h>
#include <frog/types.h>

#define USER_SMOKE_MAGIC 0x46524f47U

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

void _start(void) __attribute__((noreturn));

void _start(void)
{
        bool passed = raw_syscall0(SYS_NR_COUNT + 17) == -ENOSYS;

        passed = (raw_syscall0(SYS_STAT) == -ENOSYS) && passed;
        raw_syscall1(SYS_TESTSYSCALL, passed ? USER_SMOKE_MAGIC : 0);

        for (;;)
                __asm__ volatile("pause");
}
