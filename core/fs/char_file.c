#include <debug.h>
#include <device/ide.h>
#include <fs/char_file.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <ioqueue.h>
#include <math.h>
#include <sys/memory.h>
#include <string.h>

extern struct file g_file_table[MAX_FILE_OPEN];
extern struct lock g_ft_lock;

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
int_32 char_file_create(struct partition *part,
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
    if (flush_dir_entry(part, parent_d, &new_entry, buf)) {
        // TODO:
        //  kprint("Failed at flush directory entry");
        rollback_step = 3;
        // Recover! Need recover inode bitmap set
        //        ! free new_f_inode
        //        ! clear g_file_table[fd_idx]
        goto roll_back;
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

    // 8. add new inode to open_inodes
    list_add(&new_f_inode->inode_tag, &part->open_inodes);

    sys_free(buf);

    return fd_idx;

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


int_32 open_char_file(struct partition *part, uint_32 inode_nr, uint_8 flags)
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
    lock_release(&g_ft_lock);
    if ((flags & O_RDONLY) == O_RDONLY) {
        return install_thread_fd(gidx);
    } else {
        return -1;
    }
}

int_32 close_char_file(struct file *file)
{
    if (file == NULL) {
        return -1;
    }
    file->fd_inode->i_lock = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}

uint_32 read_char_file(int_32 fd, void *buf, uint_32 count)
{
    int_32 g_fd = fd_local2global(fd);
    CircleQueue *memory = (CircleQueue*) g_file_table[g_fd].fd_inode->i_zones[0];
    char data = ioqueue_get_data(memory);
    memcpy(buf, &data, 1);
    return 1;
}

uint_32 write_char_file(int_32 fd, const void *buf, uint_32 count)
{
    int_32 g_fd = fd_local2global(fd);
    CircleQueue *memory = (CircleQueue*) g_file_table[g_fd].fd_inode->i_zones[0];
    char *data = (char*)buf;
    ioqueue_put_data(data[0], memory);
    return 1;
}
