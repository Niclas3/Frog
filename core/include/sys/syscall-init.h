#ifndef __SYS_SYSCALL_INIT
#define __SYS_SYSCALL_INIT

#include <ostype.h>
typedef struct thread_control_block TCB_t;
typedef struct kwaak_msg message;

uint_32 sys_getpid(void);

uint_32 sys_sendrec(uint_32 func,
                    uint_32 src_dest,
                    message *p_msg);
void syscall_init(void);
#endif
