#include <ioqueue.h>

#include <debug.h>


struct queue_data *new_ioqueue_data(char data){
    // TODO: finished after we have malloc() 
    PAINC("DONT USE IT FUNCTION NOT FINISHED!!");
    struct queue_data io_data = {
        .data = data,
    };
    return &io_data;
}

void init_ioqueue(CircleQueue *queue){
    lock_init(&queue->queue_lock);
    queue->producor_p = 0; // put from tail
    queue->consumor_p = 0; // get from head
}

uint_32 ioqueue_is_full(CircleQueue *queue){
    return queue->producor_p == QUEUE_MAX;
}
uint_32 ioqueue_is_empty(CircleQueue *queue){
    return queue->producor_p == queue->consumor_p;
}


void ioqueue_put_data(struct queue_data *data, CircleQueue *queue){
    // Test if queue is full or not
    if(!ioqueue_is_full(queue)){
        /* lock_fetch(&queue->queue_lock); */
        int p_idx = queue->producor_p;
        queue->buf[p_idx] = data->data;//copy data to queue
        queue->producor_p++; // producor_p pointer to next available pointer
        /* lock_release(&queue->queue_lock); */
    }else{
        /* PAINC("IO/Queue: put data from a full queue"); */
        /* lock_fetch(&queue->queue_lock); */
        queue->producor_p = 0;
        queue->buf[queue->producor_p] = data->data;//copy data to queue
        queue->producor_p++; // producor_p pointer to next available pointer
        /* lock_release(&queue->queue_lock); */
    }
}

// @return: int is a error code 
//          if 0 ok
//          if !0 error
int ioqueue_get_data(struct queue_data *data, CircleQueue *queue){
    // Test if queue is empty or not
    // ! Is there GC problem?
    if(!ioqueue_is_empty(queue)){
        if(queue->consumor_p >= QUEUE_MAX){
            /* lock_fetch(&queue->queue_lock); */
            queue->consumor_p = 0;
            data->data = queue->buf[queue->consumor_p];
            queue->consumor_p++;
            /* lock_release(&queue->queue_lock); */
            return 0;
        }else{
            /* lock_fetch(&queue->queue_lock); */
            data->data = queue->buf[queue->consumor_p];
            queue->consumor_p++;
            /* lock_release(&queue->queue_lock); */
            return 0;
        }
    }else{
        return -1;
        /* PAINC("IO/Queue: get data from a empty queue"); */
    }
}


