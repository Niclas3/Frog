#include <ioqueue.h>
#include <sys/int.h>

#include <debug.h>


struct queue_data *new_ioqueue_data(char data)
{
    // TODO: finished after we have malloc()
    PAINC("DONT USE IT FUNCTION NOT FINISHED!!");
    struct queue_data io_data = {
        .data = data,
    };
    return &io_data;
}

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


void ioqueue_put_data(struct queue_data *data, CircleQueue *queue)
{
   /* ASSERT(intr_get_status() == INTR_OFF); */
    // Test if queue is full or not
    if (ioqueue_is_full(queue)) {
        /* PAINC("IO/Queue: put data from a full queue"); */
        lock_fetch(&queue->queue_lock);
        ioqueue_wait(&queue->producer);
        lock_release(&queue->queue_lock);
    }
    int p_idx = queue->producer_p;
    queue->buf[p_idx] = data->data;  // copy data to queue
    queue->producer_p++;  // producer_p pointer to next available pointer
    if (queue->consumor != NULL) {
        wakeup(&queue->consumor);
    }
}

// @return: int is a error code
//          if 0 ok
//          if !0 error
char ioqueue_get_data(struct queue_data *data, CircleQueue *queue)
{
   /* ASSERT(intr_get_status() == INTR_OFF); */
    // Test if queue is empty or not
    // ! Is there GC problem?
    if (ioqueue_is_empty(queue)) {
        lock_fetch(&queue->queue_lock);
        ioqueue_wait(&queue->consumor);
        lock_release(&queue->queue_lock);
    }
    data->data = queue->buf[queue->consumor_p];
    queue->consumor_p++;
    if(queue->producer != NULL){
        wakeup(&queue->producer);
    }
    return queue->buf[queue->consumor_p];
}
