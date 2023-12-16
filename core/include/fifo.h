#ifndef __LIB_FIFO_H
#define __LIB_FIFO_H
#include <ostype.h>

#define FIFO_QUEUE_MAX 1024
#define FIFO_LAST_POS FIFO_QUEUE_MAX-1
typedef struct fifo_queue{
    int put_p;
    int get_p;
    char *buf;
    uint_32 size;
    uint_32 free;
} FIFO;

void init_fifo(FIFO *fifo_queue, uint_32 size, char* buf);

uint_32 fifo_put_data(char data, FIFO *queue);

char fifo_get_data(FIFO *queue);

uint_32 fifo_length(FIFO *queue);
uint_32 fifo_rest(FIFO *queue);

#endif
