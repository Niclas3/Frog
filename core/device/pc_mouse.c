#include <device/pc_mouse.h>
#include <errno-base.h>
#include <string.h>
#include <math.h>

#include <sys/fork.h>
#include <sys/int.h>
#include <sys/memory.h>
#include <sys/threads.h>
#include <sys/sched.h>

#include <device/ide.h>
#include <device/devno-base.h>

#include <fs/fcntl.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>

#include <frog/poll.h>

#include <debug.h>

extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;

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

    enum intr_status old_status = intr_disable();

    uint_32 packet_size = sizeof(mouse_device_packet_t);
    memcpy(packet, &queue->buf[queue->tail], packet_size);

    queue->tail = (queue->tail + packet_size) & (MOUSE_PKG_BUF_SIZE - 1);

    intr_set_status(old_status);
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

/**
 * create a char type file
 *
 * @return reture a global file table index
 *****************************************************************************/
int_32 pcmouse_create(struct partition *part,
                      struct dir *parent_d,
                      char *name,
                      void *target)
{
    uint_8 rollback_step = 0;
    char *buf = sys_malloc(1024);
    if (!buf) {
        //  kprint("Not enough memory for io buf");
        return -1;
    }

    // 1. Need new inode aka create a inode (inode_open())
    uint_32 inode_nr = inode_bitmap_alloc(part);
    if (inode_nr == -1) {
        // TODO:
        //  kprint("Not enough inode bitmap position.");
        return -1;
    }
    ASSERT(inode_nr != -1);
    struct inode *new_f_inode = sys_malloc(sizeof(struct inode));
    if (!new_f_inode) {
        // TODO:
        //  kprint("Not enough memory for inode .");
        // Recover! Need recover inode bitmap set
        rollback_step = 1;
        goto roll_back;
    }
    new_inode(inode_nr, new_f_inode);

    new_f_inode->i_mode = FT_CHAR << 11;
    new_f_inode->i_zones[0] = (uint_32 *) target;
    new_f_inode->i_count++;
    new_f_inode->i_dev = DNOPCMOUSE;
    // 2. new dir_entry
    struct dir_entry new_entry;
    new_dir_entry(name, inode_nr, FT_CHAR, &new_entry);
    // 3.get file slot form global file_table
    lock_fetch(&g_ft_lock);
    // global file table index
    uint_32 fd_idx = occupy_file_table_slot();
    if (fd_idx == -1) {
        // TODO:
        //  kprint("Not enough slot at file table.");
        // Recover! Need recover inode bitmap set
        //        ! free new_f_inode
        rollback_step = 2;
        goto roll_back;
    }

    g_file_table[fd_idx].fd_pos = 0;
    g_file_table[fd_idx].fd_inode = new_f_inode;
    g_file_table[fd_idx].fd_inode->i_lock = false;
    lock_release(&g_ft_lock);

    // 4. flush dir_entry to parents directory
    // search new entry first
    int_32 failed = search_dir_entry(part, name, parent_d, &new_entry);
    if (!failed) {
        list_add(&new_f_inode->inode_tag, &part->open_inodes);
        sys_free(buf);
        return fd_idx;
    } else {
        if (flush_dir_entry(part, parent_d, &new_entry, buf)) {
            // TODO:
            //  kprint("Failed at flush directory entry");
            rollback_step = 3;
            // Recover! Need recover inode bitmap set
            //        ! free new_f_inode
            //        ! clear g_file_table[fd_idx]
            goto roll_back;
        }
        list_add(&new_f_inode->inode_tag, &part->open_inodes);
        sys_free(buf);
        return fd_idx;
    }

    // 5. flush parent inode
    //  flush_dir_entry will change parent directory inode size
    /* memset(buf, 0, 1024);  // 1024 for 2 sectors */
    /* flush_inode(part, parent_d->inode, buf); */
    /* 6. flush new inode */
    /* memset(buf, 0, 1024);  // 1024 for 2 sectors */
    /* flush_inode(part, new_f_inode, buf); */

    // 7. flush inode bitmap and zones bitmap
    /* flush_bitmap(part, INODE_BITMAP, inode_nr); */


roll_back:
    switch (rollback_step) {
    case 3:
        memset(&g_file_table[fd_idx], 0, sizeof(struct file));
    case 2:
        sys_free(new_f_inode);
    case 1:
        set_value_bitmap(&part->inode_bitmap, inode_nr, 0);
        break;
    }
    sys_free(buf);
    return -1;
}

uint_32 read_pcmouse(struct file *file, void *buf, uint_32 count)
{
    ASSERT(count % sizeof(mouse_device_packet_t) == 0);
    TCB_t *cur = running_thread();
    DECLARE_WAITQUEUE(wait, cur);
    uint_32 index = count / sizeof(mouse_device_packet_t);
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
        enum intr_status old_status = intr_disable();
        cur->status = THREAD_TASK_READY;
        remove_wait_queue(&queue->proc_list, &wait);
        intr_set_status(old_status);
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

uint_32 poll_pcmouse(struct file *file, poll_table *wait)
{
    // add this file to waiting list
    poll_wait(file, &queue->proc_list, wait);
    if (!queue_empty())
        return POLLIN | POLLRDNORM;
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
