#ifndef __SYS_SYSCALL_H
#define __SYS_SYSCALL_H
#include <ostype.h>

enum SYSCALL_NR{
    SYS_getpid, // 0
    SYS_write,  // 1
};

uint_32 getpid(void);

//System call: uint_32 write(char*)
//return len of str
uint_32 write(char *str);

#endif
