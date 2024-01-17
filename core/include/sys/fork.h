#pragma once

#include <ostype.h>
#include <sys/wait.h>

void add_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
uint_32 sys_fork(void);
