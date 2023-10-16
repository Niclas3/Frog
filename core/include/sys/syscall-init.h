#ifndef __SYS_SYSCALL_INIT
#define __SYS_SYSCALL_INIT

#include <ostype.h>
uint_32 sys_getpid(void);
void syscall_init(void);
#endif
