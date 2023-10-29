#ifndef __SYS_SYSCALL_H
#define __SYS_SYSCALL_H
#include <ostype.h>
#include <ipc.h>

typedef struct kwaak_msg message;

enum SYSCALL_NR{
    SYS_getpid, // 0
    SYS_write,  // 1
    SYS_sendrec,
};

uint_32 getpid(void);

pid_t get_pid(void);
uint_32 get_ticks(void);

//System call: uint_32 write(char*)
//return len of str
uint_32 write(char *str);

uint_32 sendrec(uint_32 func, uint_32 src_dest, message* p_msg);
#endif
