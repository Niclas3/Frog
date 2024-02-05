#ifndef __IOQUEUE_H
#define __IOQUEUE_H

#include <list.h>
#include <sys/semaphore.h>
#include <sys/threads.h>

#define QUEUE_MAX 4000  // 1 page size is 4096
typedef struct ioqueue{
    struct lock queue_lock;
    struct list_head producer_waiting_list;
    struct list_head consumor_waiting_list;
    int_32 producer_p;
    int_32 consumor_p;
    uint_32 size;
    char buf[];
} CircleQueue;


CircleQueue *init_ioqueue(uint_32 size);
void destory_ioqueue(CircleQueue *queue);

uint_32 ioqueue_put_data(CircleQueue *queue, char* data, uint_32 count);
uint_32 ioqueue_get_data(CircleQueue *queue, char* data, uint_32 count);

// check if ioqueue is available 
// return 0 has data unread
// return -1 no data unread
int_32 ioqueue_check(CircleQueue *queue);

uint_32 ioqueue_avaliable(CircleQueue *queue);
int ioqueue_size(CircleQueue *queue);

// return !0 when queue is full
uint_32 ioqueue_is_full(CircleQueue *queue);
// return !0 when queue is empty
uint_32 ioqueue_is_empty(CircleQueue *queue);

#endif 
