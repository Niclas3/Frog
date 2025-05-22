#include <device/ps2_ports.h>
#include <frog/interrupt.h>
#include <frog/types.h>
#include <kernel/device.h>
#include <kernel/driver.h>

#include <frog/irqflags.h>
#include <kernel/assert.h>
#include <kernel/chardev.h>
#include <kernel/debug.h>
#include <kernel/dev.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>


#include <frog/errno.h>
#include <frog/fork.h>
#include <frog/poll.h>
#include <frog/sched.h>
#include <frog/string.h>
#include <frog/threads.h>

#include "keymap.h"

#define KBD_BUF_SIZE 2048

extern struct bus_type isa_bus;

struct ps2kbd_queue {
        unsigned long head;
        unsigned long tail;
        wait_queue_head_t proc_list;
        unsigned char buf[KBD_BUF_SIZE];
};
static struct ps2kbd_queue *queue;


int ps2_kbd_probe(struct device *dev);

int_32 ps2_kbd_open(struct inode *inode, struct file *file);
int_32 ps2_kbd_close(struct file *file);
int_32 ps2_kbd_read(struct file *file, void *buf, uint_32 count);
int_32 ps2_kbd_write(struct file *file, const void *buf, uint_32 count);
uint_32 ps2_kbd_poll(struct file *file, struct poll_table_struct *wait);
int_32 ps2_kbd_ioctl(struct file *file, uint_32 request, void *argp);

static uint_8 get_from_queue(void)
{
        unsigned char result;

        unsigned long flags;
        local_irq_save(flags);
        result = queue->buf[queue->tail];
        queue->tail = (queue->tail + 1) & (KBD_BUF_SIZE - 1);
        local_irq_restore(flags);
        return result;
}

static inline bool queue_empty()
{
        return queue->head == queue->tail;
}

static inline int_32 queue_size()
{
        return (queue->head - queue->tail) & (KBD_BUF_SIZE - 1);
}

static boolean ctrl_status, shift_status, alt_status, caps_lock_status,
    meta_status, ext_scancode;

static inline void handle_keyboard_event(uint_16 scan_code)
{
        bool is_break_code;
        bool ctrl_pressed = ctrl_status;
        bool shift_pressed = shift_status;
        bool cap_lock_pressed = caps_lock_status;
        bool meta_pressed = meta_status;

        if (scan_code == FLAG_EXT) {  // scan_code == 0xe0
                ext_scancode = true;
                return;
        }
        if (ext_scancode) {
                scan_code = 0xe000 | scan_code;
                ext_scancode = false;
        }

        is_break_code = (scan_code & 0x0080) != 0;
        if (is_break_code) {
                uint_16 make_code =
                    (scan_code &= 0xff7f);  // break_code = 0x80+make_code
                if (make_code == MAKE_CTRL_R || make_code == MAKE_CTRL_L) {
                        ctrl_status = false;
                } else if (make_code == MAKE_ALT_R || make_code == MAKE_ALT_L) {
                        alt_status = false;
                } else if (make_code == MAKE_SHIFT_R ||
                           make_code == MAKE_SHIFT_L) {
                        shift_status = false;
                } else {
                }
                return;
        } else if ((scan_code > 0x00 && scan_code < 0x3b) ||
                   (scan_code == MAKE_ALT_R || scan_code == MAKE_CTRL_R) ||
                   (scan_code == MAKE_META_L || scan_code == MAKE_META_R)) {
                bool shift = false;

                /*   0x02 -> 1
                 *   0x0d -> =
                 *   0x0e -> backspace
                 * */
                if ((scan_code >= 0x02 && scan_code < 0x0e) ||
                    (scan_code == 0x1a) ||  // '['
                    (scan_code == 0x1b) ||  // ']'
                    (scan_code == 0x27) ||  // ';'
                    (scan_code == 0x28) ||  // '\''
                    (scan_code == 0x29) ||  // '`'
                    (scan_code == 0x2b) ||  // '\\'
                    (scan_code == 0x33) ||  // ','
                    (scan_code == 0x34) ||  // '.'
                    (scan_code == 0x35) /* '/'*/) {
                        if (shift_pressed) {
                                shift = true;
                        }
                } else {  // alphabet
                        if (shift_pressed && cap_lock_pressed) {
                                shift = false;
                        } else if (shift_pressed || cap_lock_pressed) {
                                shift = true;
                        } else {
                                shift = false;
                        }
                }

                uint_8 index = (scan_code & 0x00ff);
                char key = keymap[index][shift];
                if (key) {
                        // Process scan code to key and store key into queue
                        //
                        int head = queue->head;
                        queue->buf[head] = key;
                        head = (head + 1) & (KBD_BUF_SIZE - 1);
                        if (head != queue->tail) {  // queue is not empty
                                queue->head = head;
                                wake_up_interruptible(&queue->proc_list);
                        }
                        return;
                }

                if (scan_code == MAKE_CTRL_L || scan_code == MAKE_CTRL_R) {
                        ctrl_status = true;
                } else if (scan_code == MAKE_ALT_L || scan_code == MAKE_ALT_R) {
                        alt_status = true;
                } else if (scan_code == MAKE_SHIFT_L ||
                           scan_code == MAKE_SHIFT_R) {
                        shift_status = true;
                } else if (scan_code == MAKE_CAP_LOCK) {
                        caps_lock_status = true;
                } else if (scan_code == MAKE_META_L ||
                           scan_code == MAKE_META_R) {
                        meta_status = true;
                }

        } else {
                if (scan_code == 0x0) {
                } else {
                        PANIC("unknow key");
                }
        }
        return;
}


void ps2_kbd_ISR(void)
{
        uint_16 scan_code = 0x0;
        while (inb(PS2_STATUS) & PS2_STR_OUTPUT_BUFFER_FULL) {
                scan_code = ps2_read_byte();  // get scan_code
        }
        handle_keyboard_event(scan_code);

        ack(INT_VECTOR_KEYBOARD);
}

int_32 ps2_kbd_open(struct inode *inode, struct file *file)
{
        return 0;
}

int_32 ps2_kbd_close(struct file *file)
{
        return 0;
}

int_32 ps2_kbd_write(struct file *file, const void *buf, uint_32 count)
{
        return 0;
}

int_32 ps2_kbd_read(struct file *file, void *buf, uint_32 count)
{
        TCB_t *cur = running_thread();
        DECLARE_WAITQUEUE(wait, cur);
        uint_32 index = count;
        uint_8 c;
        if (queue_empty()) {
                if (file->f_flag & O_NONBLOCK)

                        return -EAGAIN;
                add_wait_queue(&queue->proc_list, &wait);
                thread_block(THREAD_TASK_WAITING);

                unsigned long flags;
                local_irq_save(flags);
                cur->status = THREAD_TASK_READY;
                remove_wait_queue(&queue->proc_list, &wait);
                local_irq_restore(flags);
        }
        while (index > 0 && !queue_empty()) {
                c = get_from_queue();
                memcpy(buf++, &c, 1);
                index--;
        }
        if (count - index) {
                return count - index;
        }

        return 0;
}

uint_32 ps2_kbd_poll(struct file *file, struct poll_table_struct *wait)
{
        // add this file to waiting list
        poll_wait(file, &queue->proc_list, wait);
        if (!queue_empty())
                // Trigger EPOLL_IN_EVENT callback function invoke
                return POLLIN | POLLRDNORM;
        return 0;
}

int_32 ps2_kbd_ioctl(struct file *file, uint_32 request, void *argp)
{
        return 0;
}

static struct driver ps2_kbd_driver = {.name = "ps2-kbd",
                                       .bus = &isa_bus,
                                       .probe = ps2_kbd_probe};

static struct file_operations ps2_kbd_file_operations = {
    .open = ps2_kbd_open,
    .close = ps2_kbd_close,
    .read = ps2_kbd_read,
    .write = ps2_kbd_write,
    .poll = ps2_kbd_poll,
    .ioctl = ps2_kbd_ioctl};

int ps2_kbd_probe(struct device *dev)
{
        dev->driver = &ps2_kbd_driver;
        // enable keyboard
        ps2_wait_writeable();
        outb(PS2_COMMAND, KBD_WRITE);
        ps2_wait_writeable();
        outb(PS2_DATA, KBDC_MODE);

        register_r0_intr_handler(INT_VECTOR_KEYBOARD,
                                 (Inthandle_t *) ps2_kbd_ISR);
        // init kbd_queue
        queue = (struct ps2kbd_queue *) kmalloc(sizeof(*queue));
        if (queue == NULL) {
                PANIC("[ps2kbd]: no memory for kbd queue");
                return -ENOMEM;
        }
        memset(queue, 0, sizeof(*queue));
        queue->head = queue->tail = 0;
        init_waitqueue_head(&queue->proc_list);

        // add this ps2 kbd to chrdev list

        int major = register_chrdev(0, &ps2_kbd_file_operations);
        ASSERT(major != -1);
        if (devfs_create_node("input/event0",DEV_TYPE_CHAR, major, 0) == -1) {
                WARN("[ps2/kbd driver]: some wrong at create a devfs node.");
        }

        return 0;
}

uint_32 ps2_kbd_driver_init(void)
{
        register_driver(&ps2_kbd_driver);
        return 0;
}
