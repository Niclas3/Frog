#ifndef _FROG_FORK_H
#define _FROG_FORK_H
#include <frog/types.h>
#include <frog/wait.h>

void add_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
pid_t sys_fork(void);

#endif
