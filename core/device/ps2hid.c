#include <device/devno-base.h>
#include <device/pc_mouse.h>
#include <device/pc_kbd.h>
#include <device/ps2hid.h>

#include <frog/poll.h>

#include <io.h>

#include <protect.h>
#include <sys/int.h>

#include <errno-base.h>

#include <debug.h>
#include <sys/2d_graphics.h>

#include <device/ide.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <math.h>
#include <string.h>
#include <sys/fork.h>
#include <sys/memory.h>
#include <sys/threads.h>
#include <sys/sched.h>

extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;


struct aux_queue {
    unsigned long head;
    unsigned long tail;
    wait_queue_head_t proc_list;
    unsigned char buf[AUX_BUF_SIZE];
};

static struct aux_queue *queue;  // mouse queue

// TODO:
// Use pipe replace ioqueue
/* int_32 g_kbd_pipe_fd[2]; */
/* int_32 g_mouse_pipe_fd[2]; */


/**
 * Wait PS/2 controller's output buffer is filled.
 *
 * Use it before READING from the controller.
 * *****************************************************************************/
static int_8 ps2_wait_output(void)
{
    uint_32 timeout = 100000;
    while (--timeout) {
        /* PS2_STR_OUTPUT_BUFFER_FULL; */
        if (inb(PS2_STATUS) & 0x1)
            return 0;
    }
    return 1;
}

/**
 * Wait PS/2 controller's input buffer is filled.
 *
 * Use it before WRITING to the controller.
 *
 *****************************************************************************/
static int_8 ps2_wait_input(void)
{
    uint_32 timeout = 100000;
    while (--timeout) {
        /* PS2_STR_SEND_NOTREADY */
        if (inb(PS2_STATUS) & (0x1 << 1))
            return 0;
    }
    return 1;
}


/**
 * Send a command with no response or argument
 *
 *****************************************************************************/
static void ps2_command(uint_8 cmd)
{
    ps2_wait_input();
    outb(PS2_COMMAND, cmd);
}

/**
 * Send a command with response
 *****************************************************************************/
static uint_8 ps2_command_response(uint_8 cmd)
{
    ps2_wait_input();
    outb(PS2_COMMAND, cmd);
    ps2_wait_output();
    return inb(PS2_DATA);
}

/**
 * Send a command with argument but no response
 *****************************************************************************/
static void ps2_command_arg(uint_8 cmd, uint_8 arg)
{
    ps2_wait_input();
    outb(PS2_COMMAND, cmd);
    ps2_wait_output();
    outb(PS2_DATA, arg);
}

/**
 * Read from ps2 data
 *****************************************************************************/
static uint_8 ps2_read_byte(void)
{
    ps2_wait_output();
    return inb(PS2_DATA);
}

/**
 * Communicate with PS2 keyboard
 *****************************************************************************/
static uint_8 kbd_write(uint_8 data)
{
    ps2_wait_input();
    outb(PS2_DATA, data);
    ps2_wait_output();
    return inb(PS2_DATA);
}

static uint_8 mouse_write(uint_8 data)
{
    ps2_command_arg(MOUSE_WRITE, data);
    ps2_wait_output();
    return inb(PS2_DATA);
}

static inline void handle_mouse_event(char scancode)
{
    int head = queue->head;
    queue->buf[head] = scancode;
    head = (head + 1) & (AUX_BUF_SIZE - 1);
    if (head != queue->tail) {  // queue is not empty
        queue->head = head;
        wake_up_interruptible(&queue->proc_list);
    }
}


/* int 0x2C;
 * Interrupt handler for PS/2 mouse
 **/
void inthandler2C(void)
{
    char scancode = inb(PS2_DATA);
    // send scancode to make mouse package
    handle_ps2_mouse_scancode(scancode);
    // send scancode to this file queue
    handle_mouse_event(scancode);
    return;
}

/*
 * int 0x21;
 * Interrupt handler for Keyboard
 **/
void inthandler21(void)
{
    uint_16 scan_code = 0x0;
    while (inb(PS2_STATUS) & PS2_STR_OUTPUT_BUFFER_FULL) {
        scan_code = ps2_read_byte();  // get scan_code
    }
    handle_keyboard_event(scan_code);
}


bool is_char_file(struct partition *part, int_32 inode_nr)
{
    struct inode *inode_char_file = inode_open(part, inode_nr);
    return IS_FT_CHAR(inode_char_file);
}

bool is_char_fd(int_32 fd)
{
    uint_32 g_fd = fd_local2global(fd);
    return IS_FT_CHAR(g_file_table[g_fd].fd_inode);
}


/**
 * create a char type file
 *
 * @return reture a global file table index
 *****************************************************************************/
int_32 aux_create(struct partition *part,
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
    new_f_inode->i_dev = DNOAUX;
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


int_32 open_aux(struct partition *part, uint_32 inode_nr, uint_8 flags)
{
    lock_fetch(&g_ft_lock);
    int_32 gidx = occupy_file_table_slot();
    if (gidx == -1) {
        // TODO:
        // kprint("Not enough global file table slots. when open file");
        return -1;
    }
    g_file_table[gidx].fd_inode = inode_open(part, inode_nr);
    g_file_table[gidx].fd_pos = 0;
    g_file_table[gidx].fd_flag = flags;
    queue->head = queue->tail = 0; /* Flush input queue */
    lock_release(&g_ft_lock);
    if ((flags & O_RDONLY) == O_RDONLY) {
        return install_thread_fd(gidx);
    } else {
        return -1;
    }
}

int_32 close_aux(struct file *file)
{
    if (file == NULL) {
        return -1;
    }
    file->fd_inode->i_lock = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}

static uint_8 get_from_queue(void)
{
    unsigned char result;
    unsigned long flags;

    enum intr_status old_status = intr_disable();
    result = queue->buf[queue->tail];
    queue->tail = (queue->tail + 1) & (AUX_BUF_SIZE - 1);
    intr_set_status(old_status);
    return result;
}

static inline bool queue_empty()
{
    return queue->head == queue->tail;
}

uint_32 read_aux(struct file *file, void *buf, uint_32 count)
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

uint_32 write_aux(struct file *file, const void *buf, uint_32 count)
{
    /* CircleQueue *queue = (CircleQueue *) file->fd_inode->i_zones[0]; */
    /* char *data = (char *) buf; */
    /* if (ioqueue_is_full(queue)) { */
    /*     return 0; */
    /* } else { */
    /*     ioqueue_put_data(data[0], queue); */
    /*     return 1; */
    /* } */
    return count;
}

uint_32 aux_poll(struct file *file, poll_table * wait)
{
    // add this file to waiting list
	poll_wait(file, &queue->proc_list, wait);
	if (!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}

/**
 * Initialze i8042/AIP PS/2 controller.
 */
void ps2hid_init(void)
{
    // init mouse queue
    queue = (struct aux_queue *) sys_malloc(sizeof(*queue));
    if (queue == NULL) {
        // TODO:
        //
        /* printk(KERN_ERR "psaux_init(): out of memory\n"); */
        /* misc_deregister(&psaux_mouse); */
        /* return -ENOMEM; */
        return;
    }
    memset(queue, 0, sizeof(*queue));
    queue->head = queue->tail = 0;
    init_waitqueue_head(&queue->proc_list);

    pc_mouse_init();
    pc_kbd_init();

    sys_char_file("/dev/input/event0", DNOPCKBD, NULL);
    sys_char_file("/dev/input/event1", DNOPCMOUSE, NULL);

    // enable keyboard
    ps2_wait_input();
    outb(PS2_COMMAND, KBD_WRITE);

    ps2_wait_input();
    outb(PS2_DATA, KBDC_MODE);
    register_r0_intr_handler(INT_VECTOR_KEYBOARD, (Inthandle_t *) inthandler21);

    // enable ps2 mouse
    register_r0_intr_handler(INT_VECTOR_PS2_MOUSE,
                             (Inthandle_t *) inthandler2C);
    ps2_wait_input();
    outb(PS2_COMMAND, MOUSE_WRITE);
    ps2_wait_input();
    outb(PS2_DATA, MOUSE_ENABLE);
}
