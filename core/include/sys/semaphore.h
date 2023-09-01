#ifndef __THREAD_SEMAPHORE_H
#define __THREAD_SEMAPHORE_H
#include <ostype.h>
#include <list.h>
#include <sys/threads.h>

struct semaphore {
    uint_32 value;
    struct list_head *waiting_queue;
};

struct lock {
    TCB_t *holder;
    struct semaphore semaphore;
    uint_32 holder_repeat_nr;
};

void semaphore_init(struct semaphore *sema, uint_32 value);

void lock_init(struct lock *lock);

void semaphore_down(struct semaphore *sema);
void semaphore_up(struct semaphore *sema);

void lock_fetch(struct lock *lock);
void lock_release(struct lock *lock);

#endif
