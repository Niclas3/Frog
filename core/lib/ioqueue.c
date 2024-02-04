#include <debug.h>
#include <ioqueue.h>
#include <math.h>
#include <sys/int.h>


static uint_32 ioqueue_avaliable(CircleQueue *queue)
{
    uint_32 len = 0;
    if (queue->consumor_p == queue->producer_p) {
        len = queue->size - 1;
        return len;
    }

    if (queue->consumor_p > queue->producer_p) {
        len = queue->consumor_p - queue->producer_p - 1;
    } else {
        len = (queue->size - queue->producer_p) + queue->consumor_p - 1;
    }
    return len;
}

static inline uint_32 ioqueue_unread(CircleQueue *queue)
{
    if (queue->consumor_p == queue->producer_p) {
        return 0;
    }
    if (queue->consumor_p > queue->producer_p) {
        return (queue->size - queue->consumor_p) + queue->producer_p;
    } else {
        return (queue->producer_p - queue->consumor_p);
    }
}



int_32 ioqueue_check(CircleQueue *queue)
{
    if (ioqueue_unread(queue) > 0) {
        return 0;
    }
    return -1;
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

static uint_32 next_index(CircleQueue *q, uint_32 current_index)
{
    return (current_index + 1) % q->size;
}

static inline void ____wake_up(struct list_head *waiting_list)
{
    enum intr_status old_status = intr_disable();
    struct list_head *pos;

    while (list_length(waiting_list) > 0) {
        struct list_head *pos = list_pop(waiting_list);
        TCB_t *waiter = list_entry(pos, TCB_t, general_tag);
        thread_unblock(waiter);
    }

    intr_set_status(old_status);
}

static void wake_up_consumors(CircleQueue *queue)
{
    if (!list_is_empty(&queue->consumor_waiting_list)) {
        ____wake_up(&queue->consumor_waiting_list);
    }
}

static void wake_up_producers(CircleQueue *queue)
{
    if (!list_is_empty(&queue->producer_waiting_list)) {
        ____wake_up(&queue->producer_waiting_list);
    }
}

static void add_to_waiting_list(TCB_t *cur, struct list_head *waiting_list)
{
    if (list_find_element(&cur->general_tag, waiting_list)) {
        PANIC("waiting thread already in waiting list?!");
    }
    list_add_tail(&cur->general_tag, waiting_list);
}


// return written size
uint_32 ioqueue_put_data(CircleQueue *queue, char *data, uint_32 count)
{
    // Test if queue is full or not
    // if queue is full put current process to producer_waitiing_list
    uint_32 written = 0;
    while (written < count) {
        lock_fetch(&queue->queue_lock);
        char *w_data = data;
        if (ioqueue_avaliable(queue) > count) {
            // write data to queue
            while (ioqueue_avaliable(queue) > 0 && written < count) {
                queue->buf[queue->producer_p] = *w_data;
                w_data++;
                written++;
                queue->producer_p = next_index(queue, queue->producer_p);
            }
        }
        // wake up process in consumer list
        wake_up_consumors(queue);
        if (written < count) {
            // add this process to queue->producer_waiting_list
            TCB_t *cur = running_thread();
            add_to_waiting_list(cur, &queue->producer_waiting_list);
            enum intr_status old_status = intr_disable();
            thread_block(THREAD_TASK_WAITING);
            lock_release(&queue->queue_lock);
            intr_set_status(old_status);
        } else {
            lock_release(&queue->queue_lock);
        }
    }
    return written;
}

uint_32 ioqueue_get_data(CircleQueue *queue, char *data, uint_32 count)
{
    uint_32 read_size = 0;
    char *rd_cursor = data;
    while (read_size < count) {
        lock_fetch(&queue->queue_lock);
        // test queue length is larger than 0
        if (ioqueue_unread(queue) >= count) {
            while (ioqueue_unread(queue) > 0 && read_size < count) {
                *rd_cursor = queue->buf[queue->consumor_p];
                rd_cursor++;
                read_size++;
                queue->consumor_p = next_index(queue, queue->consumor_p);
            }
        }
        // wake up all process at queue->producer_waiting_list
        wake_up_producers(queue);
        // if read_size < count wait current process
        if (read_size < count) {
            // add this process to queue->producer_waiting_list
            TCB_t *cur = running_thread();
            add_to_waiting_list(cur, &queue->consumor_waiting_list);
            lock_release(&queue->queue_lock);
            thread_block(THREAD_TASK_WAITING);
        } else {
            lock_release(&queue->queue_lock);
        }
    }

    return read_size;
}

CircleQueue *init_ioqueue(uint_32 size)
{
    int_32 pg_count = 1;
    if (size > PG_SIZE + sizeof(CircleQueue)) {
        pg_count = DIV_ROUND_UP(size, PG_SIZE + sizeof(CircleQueue));
    }
    CircleQueue *queue = get_kernel_page(pg_count);
    if (queue == NULL) {
        // Not enough memory
        return NULL;
    }
    lock_init(&queue->queue_lock);
    queue->producer_p = 0;  // put from tail
    queue->consumor_p = 0;  // get from head
    queue->size = size;

    INIT_LIST_HEAD(&queue->consumor_waiting_list);
    INIT_LIST_HEAD(&queue->producer_waiting_list);

    return queue;
}

void destory_ioqueue(CircleQueue *queue)
{
    mfree_page(MP_KERNEL, (uint_32) queue, 1);
}
