#include <sys/syscall-init.h>
#include <ostype.h>
#include <sys/threads.h>
#include <sys/syscall.h>

#define syscall_max_nr 32
typedef void* syscall;

syscall syscall_table[syscall_max_nr];

uint_32 sys_getpid(void){
    return running_thread()->pid;
}

void syscall_init(void){
    syscall_table[SYS_getpid] = sys_getpid;
}

