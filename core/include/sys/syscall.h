#ifndef __SYS_SYSCALL_H
#define __SYS_SYSCALL_H
#include <ostype.h>

enum SYSCALL_NR{
    SYS_getpid
};

uint_32 getpid(void);

#endif
