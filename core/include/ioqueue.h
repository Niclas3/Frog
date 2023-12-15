#ifndef __IOQUEUE_H
#define __IOQUEUE_H

#include <list.h>
#include <sys/semaphore.h>
#include <sys/threads.h>

#define QUEUE_MAX 4000  // 1 page size is 4096
typedef struct ioqueue{
    struct lock queue_lock;
    TCB_t *producer;
    TCB_t *consumor;
    int producer_p;
    int consumor_p;
    char buf[QUEUE_MAX];
} CircleQueue;

struct queue_data {
    char   data;
    struct list_head tag;
};

void init_ioqueue(CircleQueue *queue);

void ioqueue_put_data(char data, CircleQueue *queue);
char ioqueue_get_data(CircleQueue *queue);

uint_32 ioqueue_length(CircleQueue *queue);

// return !0 when queue is full
uint_32 ioqueue_is_full(CircleQueue *queue);
// return !0 when queue is empty
uint_32 ioqueue_is_empty(CircleQueue *queue);

#endif 
