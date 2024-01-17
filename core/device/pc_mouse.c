#include <device/pc_mouse.h>
#include <errno-base.h>
#include <string.h>
#include <sys/fork.h>
#include <sys/int.h>
#include <sys/memory.h>
#include <sys/threads.h>
#include <fs/file.h>
#include <fs/fcntl.h>

#include <debug.h>

struct pc_mouse_pkg_queue {
    unsigned long head;
    unsigned long tail;
    wait_queue_head_t proc_list;
    unsigned char buf[MOUSE_PKG_BUF_SIZE];
};
static struct pc_mouse_pkg_queue *queue;

static uint_32 mouse_mode = MOUSE_DEFAULT;

struct mouse_raw_data {
    uint_8 buf[4];
    uint_8 stage;  // mouse decoding process
};

static struct mouse_raw_data mdata = {0};


static mouse_device_packet_t get_from_queue(void)
{
    mouse_device_packet_t packet;

    enum intr_status old_status = intr_disable();

    uint_32 packet_size = sizeof(mouse_device_packet_t);
    memcpy(&packet, &queue->buf[queue->tail], packet_size);

    queue->tail = (queue->tail + packet_size) & (MOUSE_PKG_BUF_SIZE - 1);

    intr_set_status(old_status);
    return packet;
}

static inline bool queue_empty()
{
    return queue->head == queue->tail;
}

static void make_mouse_packet(struct mouse_raw_data *mdata)
{
    mdata->stage = 0;
    // Collect enough data to make a packet
    mouse_device_packet_t packet;
    uint_8 *mouse_byte = mdata->buf;
    packet.magic = MOUSE_MAGIC;
    int_32 delta_x = mdata->buf[1];
    int_32 delta_y = mdata->buf[2];
    if (delta_x && mdata->buf[0] & (1 << 4)) {
        delta_x = delta_x - 0x100;
    }
    if (delta_y && mdata->buf[0] & (1 << 5)) {
        delta_y = delta_y - 0x100;
    }
    if (mdata->buf[0] & (1 << 6) || mdata->buf[0] & (1 << 7)) {
        delta_x = 0;
        delta_y = 0;
    }

    packet.x_difference = delta_x;
    packet.y_difference = delta_y;
    packet.buttons = 0;

    if (mouse_byte[0] & 0x01) {
        packet.buttons |= LEFT_CLICK;
    }
    if (mouse_byte[0] & 0x02) {
        packet.buttons |= RIGHT_CLICK;
    }
    if (mouse_byte[0] & 0x04) {
        packet.buttons |= MIDDLE_CLICK;
    }

    if (mouse_mode == MOUSE_SCROLLWHEEL && mouse_byte[3]) {
        if ((int_8) mouse_byte[3] > 0) {
            packet.buttons |= MOUSE_SCROLL_DOWN;
        } else if ((int_8) mouse_byte[3] < 0) {
            packet.buttons |= MOUSE_SCROLL_UP;
        }
    }

    int head = queue->head;
    uint_32 packet_size = sizeof(packet);
    char *byte_packet = (char *) &packet;

    memcpy(&queue->buf[head], &packet, packet_size);
    head = (head + packet_size) & (MOUSE_PKG_BUF_SIZE - 1);

    if (head != queue->tail) { // queue is not empty
        queue->head = head;
        wake_up_interruptible(&queue->proc_list);
    }

}

void handle_ps2_mouse_scancode(uint_8 scancode)
{
    int_8 mouse_in = scancode;
    uint_8 *mouse_byte = mdata.buf;
    switch (mdata.stage) {
    case 0:
        mouse_byte[0] = mouse_in;
        if (!(mouse_in & MOUSE_V_BIT))
            break;
        ++mdata.stage;
        break;
    case 1:
        mouse_byte[1] = mouse_in;
        ++mdata.stage;
        break;
    case 2:
        mouse_byte[2] = mouse_in;
        if (mouse_mode == MOUSE_SCROLLWHEEL || mouse_mode == MOUSE_BUTTONS) {
            ++mdata.stage;
            break;
        }
        make_mouse_packet(&mdata);
        break;
    case 3:
        mouse_byte[3] = mouse_in;
        make_mouse_packet(&mdata);
        break;
    }
}

uint_32 read_pcmouse(struct file *file, void *buf, uint_32 count)
{
    ASSERT(count % sizeof(mouse_device_packet_t) == 0);
    TCB_t *cur = running_thread();
    DECLARE_WAITQUEUE(wait, cur);
    uint_32 index = count;
    mouse_device_packet_t packet;
    if (queue_empty()) {
        if (file->fd_flag & O_NONBLOCK)
            return -EAGAIN;
        add_wait_queue(&queue->proc_list, &wait);
    repeat:
        cur->status = THREAD_TASK_WAITING;
        if (queue_empty()) {
            schedule();
            goto repeat;
        }
        cur->status = THREAD_TASK_READY;
        remove_wait_queue(&queue->proc_list, &wait);
    }
    while (index > 0 && !queue_empty()) {
        packet = get_from_queue();
        memcpy(buf, &packet, sizeof(packet));
        buf += sizeof(packet);
        index--;
    }
    if (count - index) {
        return count - index;
    }
    return 0;
}

uint_32 pc_mouse_init(void)
{
    queue = (struct pc_mouse_pkg_queue *) sys_malloc(sizeof(*queue));
    if (queue == NULL) {
        // TODO:
        //
        /* printk(KERN_ERR "psaux_init(): out of memory\n"); */
        /* misc_deregister(&psaux_mouse); */
        return -ENOMEM;
    }
    memset(queue, 0, sizeof(*queue));
    queue->head = queue->tail = 0;
    init_waitqueue_head(&queue->proc_list);

    return 0;
}
