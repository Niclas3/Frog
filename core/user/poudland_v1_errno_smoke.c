#include <frog/errno.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/time.h>
#include <gua/poudland_v1.h>

void _start(void)
{
        struct poudland_v1_context context;
        struct timespec start;
        struct timespec end;
        bool passed;
        int_32 result;

        poudland_v1_context_init(&context);
        passed = clock_gettime(CLOCK_MONOTONIC, &start) == 0 &&
                 poudland_v1_connect(&context, "bad/name", 100, 0) ==
                     -EINVAL &&
                 clock_gettime(CLOCK_MONOTONIC, &end) == 0 &&
                 (end.tv_sec == start.tv_sec ||
                  (end.tv_sec == start.tv_sec + 1 &&
                   end.tv_nsec < start.tv_nsec));
        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT),
                           "b"(FROG_TEST_POUDLAND_V1_CONNECT_ERRNO),
                           "c"(passed)
                         : "memory");
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}
