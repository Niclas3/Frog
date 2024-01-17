#include <fs/select.h>
#include <fs/file.h>
#include <sys/poll.h>
#include <sys/threads.h>

extern void remove_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
extern void add_wait_queue(wait_queue_head_t *q, wait_queue_t * wait);
uint_32 sys_wait2(int n, int_32 *fds, struct timeval *tvp){
    TCB_t *cur = running_thread();
    DECLARE_WAITQUEUE(entry1, cur);
    DECLARE_WAIT_QUEUE_HEAD(wait_queue_head);

    init_waitqueue_head(&wait_queue_head);
    init_waitqueue_entry(&entry1, cur);
    add_wait_queue(&wait_queue_head, &entry1);
    remove_wait_queue(&wait_queue_head, &entry1);
    return 0;
}
