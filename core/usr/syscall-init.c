#include <sys/syscall-init.h>
#include <ostype.h>
#include <string.h>
#include <sys/syscall.h>
#include <debug.h>

#include <sys/threads.h>
#include <sys/graphic.h>

#define syscall_max_nr 32
typedef void* syscall;

syscall syscall_table[syscall_max_nr];

uint_32 sys_getpid(void){
    return running_thread()->pid;
}

uint_32 sys_write(char* str){
    // TODO: should renew this function
    draw_info((uint_8 *)0xc00a0000, 320, COL8_FFFFFF, 100, 0, str);
    /* PAINC("NOT FINISHED!"); */
    return strlen(str);
}

void syscall_init(void){
    syscall_table[SYS_getpid] = sys_getpid;
    syscall_table[SYS_write] = sys_write;
}

