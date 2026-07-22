#ifndef __SYS_PROCESS_H
#define __SYS_PROCESS_H

#include <frog/types.h>

typedef struct thread_control_block TCB_t;
struct mm_struct;
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
#define USER_IMAGE_VADDR  0x08048000

#define DEFAULT_PRIORITY 30

struct user_image {
        const void *data;
        uint_32 size;
        uint_32 load_addr;
        uint_32 entry;
};

// start a process
void process_activate(TCB_t *thread);

void page_dir_activate(TCB_t *thread);
uint_32 *create_page_dir(void);
void process_release_address_space(TCB_t *thread);

/* Build an address space which is not visible to the scheduler yet. */
struct mm_struct *process_create_user_mm(void);

/* Atomically installs a completed image and returns the replaced address space. */
int process_commit_user_image(struct mm_struct *new_mm,
                              const char *name,
                              uint_32 entry,
                              uint_32 stack,
                              uint_32 argc,
                              uint_32 argv,
                              struct mm_struct **old_mm_out);

/* Create the one parentless init process during bootstrap. */
pid_t process_execute_init_image(const struct user_image *image);

#endif
