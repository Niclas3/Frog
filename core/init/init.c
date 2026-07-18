#include <frog/syscall.h>

// first user process
void init(void)
{
        testsyscall(1);
        while (1) {
        }
}
