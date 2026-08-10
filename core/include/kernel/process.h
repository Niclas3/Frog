#ifndef __KERNEL_PROCESS_H
#define __KERNEL_PROCESS_H

#include <frog/types.h>

struct mm_struct;

struct process_startup_context {
        uint_32 entry;
        uint_32 stack;
        uint_32 argc;
        uint_32 argv;
};

/*
 * Consume owned_mm on every return. On success its ownership moves to the
 * newly published PID 1 TCB; on failure it is released here. No caller-visible
 * process exists before the final IRQ-disabled publication step.
 */
int process_publish_initial_user_mm_owned(
    struct mm_struct *owned_mm,
    const char *name,
    const struct process_startup_context *startup,
    pid_t *pid_out);

#endif
