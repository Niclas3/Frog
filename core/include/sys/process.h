#ifndef __SYS_PROCESS_H
#define __SYS_PROCESS_H

#include <ostype.h>

typedef struct thread_control_block TCB_t;
// Every user process stack address
// 0xc0000000 is bottom of stack
// 0xc0000000 is end of lower 3G in 4G virtual address
// 0xbffff000
#define USER_STACK3_VADDR 0xc0000000-0x1000
// user process code section start at USER_VADDR_START
// #define USER_VADDR_START  0x08048000
// #define USER_VADDR_START  0x01000000   // does not work at qume? i dont know why
// #define USER_VADDR_START     0x01001000
#define USER_VADDR_START  0x08000000

#define DEFAULT_PRIORITY 30

// start a process
void process_activate(TCB_t *thread);

uint_32 fork_pid(void);
void page_dir_activate(TCB_t *thread);
uint_32 *create_page_dir(void);

// Create process at ring3
void process_execute(void *filename, char *name);

// Create process at ring1
void process_execute_ring1(void *filename, char *name);

#endif
