#include <device/devno-base.h>
#include <device/ide.h>
#include <device/pc_kbd.h>

#include <errno-base.h>
#include <fs/dir.h>
#include <fs/fcntl.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <hid/keymap.h>
#include <string.h>
#include <sys/fork.h>
#include <sys/int.h>
#include <sys/memory.h>
#include <sys/threads.h>

#include <debug.h>

extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;

struct pc_kbd_queue {
    unsigned long head;
    unsigned long tail;
    wait_queue_head_t proc_list;
    unsigned char buf[KBD_BUF_SIZE];
};
static struct pc_kbd_queue *queue;

static boolean ctrl_status, shift_status, alt_status, caps_lock_status,
    meta_status, ext_scancode;

static uint_8 get_from_queue(void)
{
    unsigned char result;

    enum intr_status old_status = intr_disable();
    result = queue->buf[queue->tail];
    queue->tail = (queue->tail + 1) & (KBD_BUF_SIZE - 1);
    intr_set_status(old_status);
    return result;
}

static inline bool queue_empty()
{
    return queue->head == queue->tail;
}

/**
 * create a char type file
 *
 * @return reture a global file table index
 *****************************************************************************/
int_32 kbd_create(struct partition *part,
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
    new_f_inode->i_dev = DNOPCKBD;
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
    }

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

uint_32 read_kbd(struct file *file, void *buf, uint_32 count)
{
    TCB_t *cur = running_thread();
    DECLARE_WAITQUEUE(wait, cur);
    uint_32 index = count;
    uint_8 c;
    if (queue_empty()) {
        if (file->fd_flag & O_NONBLOCK)
            return -EAGAIN;
        add_wait_queue(&queue->proc_list, &wait);
        cur->status = THREAD_TASK_WAITING;
        schedule();
        cur->status = THREAD_TASK_READY;
        remove_wait_queue(&queue->proc_list, &wait);
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

inline void handle_keyboard_event(uint_16 scan_code)
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
        } else if (make_code == MAKE_SHIFT_R || make_code == MAKE_SHIFT_L) {
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
        } else if (scan_code == MAKE_SHIFT_L || scan_code == MAKE_SHIFT_R) {
            shift_status = true;
        } else if (scan_code == MAKE_CAP_LOCK) {
            caps_lock_status = true;
        } else if (scan_code == MAKE_META_L || scan_code == MAKE_META_R) {
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

uint_32 pc_kbd_init(void)
{
    queue = (struct pc_kbd_queue *) sys_malloc(sizeof(*queue));
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
