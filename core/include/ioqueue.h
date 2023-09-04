#ifndef __IOQUEUE_H
#define __IOQUEUE_H

#include <list.h>
#include <sys/semaphore.h>
#include <const.h>

#define QUEUE_MAX 64
typedef struct ioqueue{
    struct lock queue_lock;
    char buf[QUEUE_MAX];
    int producor_p;
    int consumor_p;
} CircleQueue;

struct queue_data {
    char   data;
    struct list_head tag;
};

struct queue_data *new_ioqueue_data(char data);
void init_ioqueue(CircleQueue *queue);

void ioqueue_put_data(struct queue_data *data, CircleQueue *queue);
int ioqueue_get_data(struct queue_data *data, CircleQueue *queue);

// return !0 when queue is full
uint_32 ioqueue_is_full(CircleQueue *queue);
// return !0 when queue is empty
uint_32 ioqueue_is_empty(CircleQueue *queue);

#endif 
