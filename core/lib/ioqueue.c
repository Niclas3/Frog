#include <ioqueue.h>
#include <sys/int.h>

#include <debug.h>

void init_ioqueue(CircleQueue *queue)
{
    lock_init(&queue->queue_lock);
    queue->consumor = NULL;
    queue->producer = NULL;
    queue->producer_p = 0;  // put from tail
    queue->consumor_p = 0;  // get from head
}

uint_32 ioqueue_is_full(CircleQueue *queue)
{
    return queue->producer_p == QUEUE_MAX;
}
uint_32 ioqueue_is_empty(CircleQueue *queue)
{
    return queue->producer_p == queue->consumor_p;
}

uint_32 ioqueue_length(CircleQueue *queue)
{
    uint_32 len = 0;
    if (queue->consumor_p >= queue->producer_p) {
        len = queue->consumor_p - queue->producer_p;
    } else {
        len = QUEUE_MAX - (queue->producer_p - queue->consumor_p);
    }
    return len;
}

static void ioqueue_wait(TCB_t **waiter)
{
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = running_thread();
    thread_block(THREAD_TASK_BLOCKED);
}
static void wakeup(TCB_t **proc)
{
    ASSERT(*proc != NULL);
    thread_unblock(*proc);
    *proc = NULL;
}

static uint_32 next_index(uint_32 current_index)
{
    return (current_index + 1) % QUEUE_MAX;
}

void ioqueue_put_data(char data, CircleQueue *queue)
{
    // Test if queue is full or not
    if (ioqueue_is_full(queue)) {
        lock_fetch(&queue->queue_lock);
        ioqueue_wait(&queue->producer);
        lock_release(&queue->queue_lock);
    }
    int p_idx = queue->producer_p;
    queue->buf[p_idx] = data;  // copy data to queue
    /* queue->producer_p++;  // producer_p pointer to next available pointer */
    queue->producer_p = next_index(queue->producer_p);
    if (queue->consumor != NULL) {
        wakeup(&queue->consumor);
    }
}

char ioqueue_get_data(CircleQueue *queue)
{
    char data;
    if (ioqueue_is_empty(queue)) {
        lock_fetch(&queue->queue_lock);
        ioqueue_wait(&queue->consumor);
        lock_release(&queue->queue_lock);
    }
    data = queue->buf[queue->consumor_p];
    /* queue->consumor_p++; */
    queue->consumor_p = next_index(queue->consumor_p);
    if (queue->producer != NULL) {
        wakeup(&queue->producer);
    }
    return data;
}
