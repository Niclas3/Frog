#include <debug.h>
#include <fs/file.h>
#include <math.h>
#include <string.h>
#include <sys/int.h>
#include <sys/semaphore.h>
#include <sys/threads.h>

#include <device/ide.h>
#include <fs/super_block.h>

#include <fs/dir.h>
#include <fs/fs.h>
#include <fs/inode.h>

#include <fs/fcntl.h>


// All files opening at same time on this system
struct file g_file_table[MAX_FILE_OPEN];
struct lock g_ft_lock;

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
    lock_fetch(&g_ft_lock);
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

    bool *write_lock = (bool *) &g_file_table[gidx].fd_inode->i_lock;
    if (flags & O_WRONLY || flags & O_RDWR) {
        enum intr_status old_status = intr_disable();
        if (!(*write_lock)) {
            *write_lock = true;
            intr_set_status(old_status);
        } else {
            intr_set_status(old_status);
            // TODO:
            // kprint("file can not be write now!");
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
int_32 file_close(struct file *file)
{
    if (file == NULL) {
        return -1;
    }
    file->fd_inode->i_lock = false;
    inode_close(file->fd_inode);
    file->fd_inode = NULL;
    return 0;
}

/**
 * write buffer to file
 *
 * @param part mounted partition
 * @param file target file
 * @param buf  buffer contain data
 * @param count writing size
 *
 * @return if success return bytes counts
 *         if failed return -1
 *****************************************************************************/
int_32 file_write(struct partition *part,
                  struct file *file,
                  const void *buf,
                  uint_32 count)
{
    // 1. test count + inode->i_size > ZONE_SIZE * 140
    if ((file->fd_inode->i_size + count) > MAX_FILE_SIZE) {
        // TODO:
        // kprint("write file error.");
        return -1;
    }
    uint_8 *io_buf = sys_malloc(1024);
    if (!io_buf) {
        // TODO:
        // kprint("write file error when no enough memory for io_buf");
        return -1;
    }

    uint_32 bytes_written = 0;
    // record write cursor
    uint_8 *w_cursor = (uint_8 *) buf;

    // 2. Test this file last writable zone. To calculate how many zones this
    // time needs.
    struct inode *f_inode = file->fd_inode;
    // Find first accessible i_zones[i]
    // 1. read all i_zones;
    uint_32 all_zones[140] = {0};
    // First 12 i_zones[] elements is direct address of data (aka dir_entry)
    for (int i = 0; i < 12 && f_inode->i_zones[i]; i++) {
        all_zones[i] = f_inode->i_zones[i];
    }
    // the 13th i_zones[] is a in-direct table which size is ZONE_SIZE bytes
    if (f_inode->i_zones[12]) {
        uint_32 *buf = (uint_32 *) sys_malloc(ZONE_SIZE);
        ide_read(part->my_disk, f_inode->i_zones[12], buf, 1);
        for (int i = 0; i < ZONE_SIZE / 4; i++) {
            all_zones[12 + i] = buf[i];
        }
        sys_free(buf);
    }

    int zone_idx;
    for (zone_idx = 0; all_zones[zone_idx]; zone_idx++)
        ;
    /* uint_32 rest_sz = f_inode->i_size > 0 ? f_inode->i_size % ZONE_SIZE : 0;
     */
    uint_32 rest_sz = f_inode->i_size % ZONE_SIZE;

    // Ready to be written block of data is chunk after filling the rest zone
    // how many zones this time need

    uint_32 chunk_cnt = 1;
    int need_filled_sz = 0;
    if (rest_sz == 0) {
        chunk_cnt = DIV_ROUND_UP(count, ZONE_SIZE);
    } else {
        need_filled_sz = ZONE_SIZE - rest_sz;
        if (count <= need_filled_sz) {
            chunk_cnt = 0;
            need_filled_sz = count;
        } else {
            chunk_cnt = DIV_ROUND_UP(count - need_filled_sz, ZONE_SIZE);
        }
    }

    /* uint_32 chunk_rest = (count - need_filled_sz) > ZONE_SIZE */
    /*                          ? (count - need_filled_sz) % ZONE_SIZE */
    /*                          : 0; */
    uint_32 chunk_rest = (count - need_filled_sz) % ZONE_SIZE;

    // Fill the Rest zone
    if (rest_sz != 0) {
        // 1. Read this zone first
        /* uint_32 zone_lba = f_inode->i_zones[zone_idx - 1]; */
        uint_32 zone_lba = all_zones[zone_idx - 1];
        ide_read(part->my_disk, zone_lba, io_buf, 1);
        // rest_sz = SIZEOF(io_buf);
        uint_8 *r_cursor = &io_buf[rest_sz];
        uint_32 len = need_filled_sz;
        ASSERT(w_cursor - (uint_8 *) buf < count);
        memcpy(r_cursor, w_cursor, len);
        w_cursor += len;
        ide_write(part->my_disk, zone_lba, io_buf, 1);

        f_inode->i_size += need_filled_sz;
        file->fd_pos += need_filled_sz;
        bytes_written += need_filled_sz;
    }

    // No rest zone, maybe first write file need allocate zone address from
    // bitmap.
    // maybe I should write file zone by zone.
    // set to f_inode->i_zones[zone_idx] = zone_lba;
    for (int i = 0; i < chunk_cnt; i++) {
        uint_32 zone_lba = zone_bitmap_alloc(part);
        if (zone_lba == -1) {
            // TODO:
            // kprint("No enough zone count when write a file");
            sys_free(io_buf);
            return -1;
        }
        flush_bitmap(part, ZONE_BITMAP, zone_lba - part->sb->s_data_start_lba);
        // direct table
        if (zone_idx + i < 12) {
            f_inode->i_zones[zone_idx + i] = zone_lba;
        } else {  // in-direct table
            // Read in-direct table
            if (zone_idx + i == 12) {
                // in-direct table address
                f_inode->i_zones[zone_idx + i] = zone_lba;
                // the zone_lba was taken to be save indirect table
                // we need a new zone_lba for saving data.
                zone_lba = zone_bitmap_alloc(part);
                if (zone_lba == -1) {
                    // TODO:
                    // kprint("No enough zone count when write a file");
                    sys_free(io_buf);
                    return -1;
                }
                flush_bitmap(part, ZONE_BITMAP,
                             zone_lba - part->sb->s_data_start_lba);
            }
            uint_32 *table = (uint_32 *) sys_malloc(ZONE_SIZE);
            if (!table) {
                // TODO:
                // kprint("No enough zone count when write a file");
                return -1;
            }
            ide_read(part->my_disk, f_inode->i_zones[12], table, 1);
            table[zone_idx - 12 + i] = zone_lba;
            ide_write(part->my_disk, f_inode->i_zones[12], table, 1);
            sys_free(table);
        }
        // write a zone buffer to this zone_lba
        ASSERT(w_cursor - (uint_8 *) buf < count);
        // If there is a rest chunk and this ture is the last turn
        if (chunk_rest > 0 && (i == (chunk_cnt - 1))) {
            // data length == chunk_rest
            uint_8 *tmp = sys_malloc(512);
            if (!tmp) {
                // TODO:
                // kprint("No enough memory when write a file");
                sys_free(io_buf);
                return -1;
            }
            memcpy(tmp, w_cursor, chunk_rest);
            ide_write(part->my_disk, zone_lba, tmp, 1);
            f_inode->i_size += chunk_rest;
            file->fd_pos += chunk_rest;
            bytes_written += chunk_rest;
            w_cursor += chunk_rest;

            sys_free(tmp);
        } else {  // ZONE_SIZE for 1 sector / 1 zone
            ide_write(part->my_disk, zone_lba, w_cursor, 1);
            f_inode->i_size += ZONE_SIZE;
            file->fd_pos += ZONE_SIZE;
            bytes_written += ZONE_SIZE;
            w_cursor += ZONE_SIZE;
        }
    }

    memset(io_buf, 0, 1024);
    flush_inode(part, f_inode, io_buf);
    sys_free(io_buf);
    return bytes_written;
}

