#include <fs/file.h>
#include <string.h>
#include <sys/threads.h>

#include <device/ide.h>
#include <fs/fs.h>
#include <fs/super_block.h>


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
 * @param g_fd_idx global file descriptor index
 * @return return idx when success
 *         return -1 when failed
 *****************************************************************************/
int_32 install_thread_fd(int_32 g_fd_idx)
{
    TCB_t *cur = running_thread();
    uint_8 fd_idx = 3;  // pass stdin, stdout, stderr
    while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
        if (cur->fd_table[fd_idx] == -1) {  // if -1 represent free slot
            cur->fd_table[fd_idx] = g_fd_idx;
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
