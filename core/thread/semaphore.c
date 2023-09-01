#include <sys/semaphore.h>
#include <sys/int.h>
#include <list.h>
#include <const.h>
#include <debug.h>

void semaphore_init(struct semaphore *sema, uint_32 value){
    sema->value = value;
    init_list_head(sema->waiting_queue);
}

void lock_init(struct lock *lock){
    lock->holder = NULL;
    lock->holder_repeat_nr = 0;
    semaphore_init(&lock->semaphore, 1);
}

// semaphore_down aka P
// P has 3 steps
// 1. test x = (sema->value > 0)
// 2. if x is true sema->value--
// 3. if x is false aka sema->value == 0,thread block self 
//    and add this thread to sema->waiting_list
//
// Also can do like that
/* test_sema_value: */
/*     if(sema->value > 0){ */
/*         sema->value--; */
/*         ASSERT(sema->value == 0); */
/*     }else{ */
/*         ASSERT(sema->value == 0); */
/*         TCB_t *cur = running_thread(); */
/*         if(list_find_element(&cur->general_tag, sema->waiting_queue)){ */
/*             PAINC("semaphore P: blocked thread already in waiting list?!"); */
/*         } */
/*         //block self then schedule to next ready thread */
/*         thread_block(SYS_THREAD_TASK_BLOCKED); */
/*     } */
/*     goto test_sema_value; */
void semaphore_down(struct semaphore *sema){
    enum intr_status old_status = intr_disable();
    while(sema->value == 0){
        ASSERT(sema->value == 0);
        TCB_t *cur = running_thread();
        if(list_find_element(&cur->general_tag, sema->waiting_queue)){
            PAINC("semaphore P: blocked thread already in waiting list?!");
        }
        //block self then schedule to next ready thread
        thread_block(SYS_THREAD_TASK_BLOCKED);
    }
    sema->value--;
    ASSERT(sema->value == 0);
    intr_set_status(old_status);
}

// Semaphore_up aka V
// 1. sema->value++
// 2. unblock thread at sema->waiting_list
void semaphore_up(struct semaphore *sema){
    enum intr_status old_status = intr_disable();
    if(list_is_empty(sema->waiting_queue)){
        struct list_head *tag = list_pop(sema->waiting_queue);
        TCB_t *blocked_thread = container_of( tag, TCB_t, general_tag);
        thread_unblock(blocked_thread);
    }
    sema->value++;
    ASSERT(sema->value == 1);
    intr_set_status(old_status);
}

// do P  aka down
void lock_fetch(struct lock *lock){
    if(lock->holder != running_thread()){
        semaphore_down(&lock->semaphore);
        lock->holder = running_thread();
        ASSERT(lock->holder_repeat_nr == 0);
        lock->holder_repeat_nr = 1;
    }else {
        lock->holder_repeat_nr++;
    }
}

// do V
void lock_release(struct lock *lock){
    if(lock->holder_repeat_nr > 1){
        lock->holder_repeat_nr--;
        return;
    }
    ASSERT(lock->holder_repeat_nr == 1);
    lock->holder = NULL;
    lock->holder_repeat_nr = 0;
    semaphore_up(&lock->semaphore);
}
