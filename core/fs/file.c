#include <debug.h>
#include <fs/file.h>
#include <string.h>
#include <sys/int.h>
#include <sys/threads.h>

#include <device/ide.h>
#include <fs/super_block.h>

#include <fs/dir.h>
#include <fs/fs.h>
#include <fs/inode.h>

#include <fs/fcntl.h>


// All files opening at same time on this system
struct file g_file_table[MAX_FILE_OPEN];

/**
 * Occupy a slot from g_file_table
 *
 * @param void
 * @return return -1 when there is no free slot left.
 *****************************************************************************/
int_32 occupy_file_table_slot(void)
{
    int_32 idx = 0;
    while (idx < MAX_FILE_OPEN) {
        if (g_file_table[idx].fd_inode == NULL) {
            break;
        }
        idx++;
    }
    if (idx == MAX_FILE_OPEN) {
        return -1;
    }
    return idx;
}

/**
 * Install given global index to current thread's fd_table[];
 *
 * @param  fd global file descriptor index
 * @return return idx when success
 *         return -1 when failed
 *****************************************************************************/
int_32 install_thread_fd(int_32 fd)
{
    TCB_t *cur = running_thread();
    uint_8 fd_idx = 3;  // pass stdin, stdout, stderr
    while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
        if (cur->fd_table[fd_idx] == -1) {  // if -1 represent free slot
            cur->fd_table[fd_idx] = fd;
            break;
        }
        fd_idx++;
    }
    if (fd_idx == MAX_FILES_OPEN_PER_PROC) {
        // TODO: kprint()
        return -1;
    }
    return fd_idx;
}

/**
 * assign a inode at inode bitmap
 *
 * @param part partition which is mounted at fs_init
 * @return return a inode number when success
 *         return -1 when failed
 *****************************************************************************/
int_32 inode_bitmap_alloc(struct partition *part)
{
    int_32 idx = find_block_bitmap(&part->inode_bitmap, 1);
    if (idx == -1) {
        return -1;
    }
    set_value_bitmap(&part->inode_bitmap, idx, 1);
    return idx;
}

/**
 * assign a zone at zone bitmap
 *
 * @param part partition which is mounted at fs_init
 * @return return zone address in lba
 *****************************************************************************/
uint_32 zone_bitmap_alloc(struct partition *part)
{
    int_32 idx = find_block_bitmap(&part->zone_bitmap, 1);
    if (idx == -1) {
        return -1;
    }
    set_value_bitmap(&part->zone_bitmap, idx, 1);
    return (part->sb->s_data_start_lba + idx);
}

/**
 * flush bitmaps to disk
 *
 * @param param write here param Comments write here
 * @return return Comments write here
 *****************************************************************************/
void flush_bitmap(struct partition *part,
                  enum bitmap_type b_type,
                  int_32 bit_idx)
{
    uint_32 off_sce = bit_idx / BITS_PER_SECTOR;
    uint_32 off_size =
        off_sce * ZONE_SIZE;  // alternative:off_size = bit_idx * 8

    uint_32 start_lba;
    uint_8 *bitmap_buf;
    if (b_type == INODE_BITMAP) {
        start_lba = part->sb->s_imap_lba + off_sce;
        bitmap_buf = part->inode_bitmap.bits + off_size;
    } else if (b_type == ZONE_BITMAP) {
        start_lba = part->sb->s_zmap_lba + off_sce;
        bitmap_buf = part->zone_bitmap.bits + off_size;
    }

    ide_write(part->my_disk, start_lba, bitmap_buf, 1);
}

/**
 * Create file
 *
 * 1. Need new inode aka create a inode (inode_open())
 *  1.1 inode_nr = inode_bitmap_alloc()
 *      inode = inode_new(inode_nr);
 *
 * 2. Create dir_entry of this file name
 *
 * 3.get file slot form global file_table
 *
 * 4. Add dir_entry to parent directory
 *
 * 5. flush new inode to disk
 *  5.1 flush_inode(inode);
 *
 * 6. flush new dir_entry to disk
 *
 * @param d create file at directory
 * @param name file name
 * @param file option flag
 * @return inode number if success
 *         -1           if failed
 *****************************************************************************/
int_32 file_create(struct partition *part,
                   struct dir *parent_d,
                   char *name,
                   uint_32 flag)
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
    // 2. new dir_entry
    struct dir_entry new_entry;
    new_dir_entry(name, inode_nr, FT_REGULAR, &new_entry);
    // 3.get file slot form global file_table
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
    g_file_table[fd_idx].fd_flag = flag;
    g_file_table[fd_idx].fd_inode = new_f_inode;
    g_file_table[fd_idx].fd_inode->i_lock = false;

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
    memset(buf, 0, 1024);  // 1024 for 2 sectors
    flush_inode(part, parent_d->inode, buf);
    // 6. flush new inode
    memset(buf, 0, 1024);  // 1024 for 2 sectors
    flush_inode(part, new_f_inode, buf);

    // 7. flush inode bitmap and zones bitmap
    flush_bitmap(part, INODE_BITMAP, inode_nr);

    // 8. add new inode to open_inodes
    list_add(&new_f_inode->inode_tag, &part->open_inodes);

    sys_free(buf);
    // 9. install new file file descriptor to current thread.
    return install_thread_fd(fd_idx);

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

/**
 * open a file when it is already created
 *
 * @param inode_nr inode number
 * @param flags file flags
 * @return a file descriptor number
 *         return -1 when failed
 *****************************************************************************/
int_32 file_open(struct partition *part, uint_32 inode_nr, uint_8 flags)
{
    // 1. get slot from global file table
    int_32 gidx = occupy_file_table_slot();
    if(gidx == -1) {
        //TODO:
        //kprint("Not enough global file table slots. when open file");
        return -1;
    }
    g_file_table[gidx].fd_inode = inode_open(part, inode_nr);
    g_file_table[gidx].fd_pos = 0;
    g_file_table[gidx].fd_flag = flags;
    bool *write_lock = (bool *)&g_file_table[gidx].fd_inode->i_lock;
    if(flags & O_WRONLY || flags & O_RDWR) {
        enum intr_status old_status = intr_disable();
        if(!(*write_lock)){
            *write_lock = true;
            intr_set_status(old_status);
        } else {
            intr_set_status(old_status);
            //TODO:
            //kprint("file can not be write now!");
            return -1;
        }
    }
    return install_thread_fd(gidx);
}

/**
 * close a file
 *
 * @param file 
 * @return -1 when failed
 *          0 when success
 *****************************************************************************/
int_32 file_close(struct file *file){
    if(file == NULL) {
        return -1;
    }
    file->fd_inode->i_lock = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}
