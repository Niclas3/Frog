#include <fifo.h>

void init_fifo(FIFO *fifo_queue, uint_32 size, char *buf)
{
    fifo_queue->put_p = 0;  // always next can put position
    fifo_queue->get_p = 0;  // always next can get position
    fifo_queue->size = size;
    fifo_queue->free = size;
    fifo_queue->buf = buf;
}

static uint_32 next_pos(uint_32 pos)
{
    return (pos + 1) % FIFO_QUEUE_MAX;
}

uint_32 fifo_put_data(char data, FIFO *queue)
{
    if (queue->free == 0) {
        return -1;
    }
    queue->buf[queue->put_p] = data;
    queue->put_p = next_pos(queue->put_p);
    queue->free--;
    return 0;
}

char fifo_get_data(FIFO *queue)
{
    char ret = queue->buf[queue->get_p];
    if (queue->free == queue->size) {
        return -1;
    }
    queue->get_p = next_pos(queue->get_p);
    queue->free++;
    return ret;
}

uint_32 fifo_rest(FIFO *queue)
{
    return queue->size - queue->free;
}

uint_32 fifo_length(FIFO *queue)
{
    uint_32 len = 0;
    if (queue->get_p >= queue->put_p) {
        len = FIFO_QUEUE_MAX - (queue->get_p - queue->put_p);
    } else {
        len = queue->put_p - queue->get_p;
    }
    return len;
}
