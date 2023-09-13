#ifndef __SYS_PROCESS_H
#define __SYS_PROCESS_H

#include <ostype.h>

typedef struct thread_control_block TCB_t;

#define USER_STACK3_VADDR 0xc0000000-0x1000
#define default_priority 30

void start_process(void *filename);

void page_dir_activate(TCB_t *thread);

uint_32* create_page_dir(void);

void process_activate(TCB_t *thread);

void process_execute(void *filename, char *name);

#endif
