#include <frog/types.h>
#include <kernel/device.h>
#include <kernel/driver.h>

#include <device/ps2_ports.h>
#include <frog/interrupt.h>

#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>

#include <kernel/chardev.h>
#include <kernel/dev.h>

#include <frog/errno.h>
#include <frog/fork.h>
#include <frog/irqflags.h>
#include <frog/poll.h>
#include <frog/string.h>
#include <frog/threads.h>


typedef enum {
        LEFT_CLICK = 0x01,
        RIGHT_CLICK = 0x02,
        MIDDLE_CLICK = 0x04,

        MOUSE_SCROLL_UP = 0x10,
        MOUSE_SCROLL_DOWN = 0x20,
} mouse_click_t;


#define MOUSE_MAGIC 0xFEED1234

#define MOUSE_DEFAULT 0
#define MOUSE_SCROLLWHEEL 1
#define MOUSE_BUTTONS 2
#ifndef MOUSE_V_BIT
#define MOUSE_V_BIT 0x08
#endif

typedef struct {
        uint_32 magic;
        int_32 x_difference;
        int_32 y_difference;
        mouse_click_t buttons;
} mouse_device_packet_t;

#define MOUSE_PKG_BUF_SIZE sizeof(mouse_device_packet_t) * 1024

extern struct bus_type isa_bus;
int ps2_mouse_probe(struct device *dev);

int_32 ps2_mouse_open(struct inode *inode, struct file *file);
int_32 ps2_mouse_close(struct file *file);
int_32 ps2_mouse_read(struct file *file, void *buf, uint_32 count);
uint_32 ps2_mouse_poll(struct file *file, struct poll_table_struct *wait);
int_32 ps2_mouse_ioctl(struct file *file, uint_32 request, void *argp);

static struct driver ps2_mouse_driver = {
    .name = "ps2-mouse",
    .probe = ps2_mouse_probe,
    .bus = &isa_bus,
};

static struct file_operations ps2_mouse_fop = {.open = ps2_mouse_open,
                                               .close = ps2_mouse_close,
                                               .read = ps2_mouse_read,
                                               .poll = ps2_mouse_poll,
                                               .ioctl = ps2_mouse_ioctl};

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


static void get_from_queue(mouse_device_packet_t *packet)
{
        unsigned long flag;
        local_irq_save(flag);

        uint_32 packet_size = sizeof(mouse_device_packet_t);
        memcpy(packet, &queue->buf[queue->tail], packet_size);

        queue->tail = (queue->tail + packet_size) & (MOUSE_PKG_BUF_SIZE - 1);

        local_irq_restore(flag);
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

        if (head != queue->tail) {  // queue is not empty
                queue->head = head;
                wake_up_interruptible(&queue->proc_list);
        }
}

static inline void handle_ps2_mouse_scancode(uint_8 scancode)
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
                if (mouse_mode == MOUSE_SCROLLWHEEL ||
                    mouse_mode == MOUSE_BUTTONS) {
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


// 0x2C
void ps2_mouse_ISR(void)
{
        uint_16 scan_code = 0x0;
        while (inb(PS2_STATUS) & PS2_STR_OUTPUT_BUFFER_FULL) {
                scan_code = ps2_read_byte();  // get scan_code
        }

        ack(INT_VECTOR_PS2_MOUSE);

        // send scancode to make mouse package
        handle_ps2_mouse_scancode(scan_code);

        irq_enter();
        irq_exit();
}

int_32 ps2_mouse_open(struct inode *inode, struct file *file)
{
        return -1;
}
int_32 ps2_mouse_close(struct file *file)
{
        return -1;
}

int_32 ps2_mouse_read(struct file *file, void *buf, uint_32 count)
{
        ASSERT(count % sizeof(mouse_device_packet_t) == 0);
        TCB_t *cur = running_thread();
        DECLARE_WAITQUEUE(wait, cur);
        uint_32 index = count / sizeof(mouse_device_packet_t);
        mouse_device_packet_t packet;
        if (queue_empty()) {
                if (file->f_flag & O_NONBLOCK)
                        return -EAGAIN;
                add_wait_queue(&queue->proc_list, &wait);
        repeat:
                cur->status = THREAD_TASK_WAITING;
                if (queue_empty()) {
                        schedule();
                        goto repeat;
                }
                unsigned long flag;
                local_irq_save(flag);

                cur->status = THREAD_TASK_READY;
                remove_wait_queue(&queue->proc_list, &wait);
                local_irq_restore(flag);
        }
        while (index > 0 && !queue_empty()) {
                get_from_queue(&packet);
                memcpy(buf, &packet, sizeof(packet));
                buf += sizeof(packet);
                index--;
        }
        if (count - index) {
                return count - index;
        }
        return 0;
}

uint_32 ps2_mouse_poll(struct file *file, struct poll_table_struct *wait)
{
        return 0;
}

int_32 ps2_mouse_ioctl(struct file *file, uint_32 request, void *argp)
{
        return 0;
}


int ps2_mouse_probe(struct device *dev)
{
        // When dev and drv match then run this function
        dev->driver = &ps2_mouse_driver;

        register_r0_intr_handler(INT_VECTOR_PS2_MOUSE,
                                 (Inthandle_t *) ps2_mouse_ISR);

        ps2_wait_writeable();
        outb(PS2_COMMAND, PS2_READ_CONFIG);
        uint_8 ctrl = inb(PS2_DATA);
        ctrl |= 0x02;  // open IRQ12(bit1)
        ps2_wait_readable();

        outb(PS2_COMMAND, PS2_WRITE_CONFIG);
        outb(PS2_DATA, ctrl);

        ps2_wait_writeable();
        outb(PS2_COMMAND, MOUSE_WRITE);
        ps2_wait_writeable();
        outb(PS2_DATA, MOUSE_ENABLE);
        ps2_wait_readable();

        // init queue
        queue = (struct pc_mouse_pkg_queue *) kmalloc(sizeof(*queue));
        if (queue == NULL) {
                /* misc_deregister(&psaux_mouse); */
                PANIC("[ps2/mouse]: not many memory for queue.");
                return -ENOMEM;
        }
        memset(queue, 0, sizeof(*queue));
        queue->head = queue->tail = 0;
        init_waitqueue_head(&queue->proc_list);
        // add to chardev
        int major = register_chrdev(0, &ps2_mouse_fop);

        if (devfs_create_node("input/event1", DEV_TYPE_CHAR, major, 0)) {
                WARN("[ps2/mouse driver]: some wrong at create a devfs node.");
        }

        return 0;
}

uint_32 ps2_mouse_driver_init(void)
{
        register_driver(&ps2_mouse_driver);
        return 0;
}

/* module_init(ps2_mouse_driver_init); */
