//----
#include <frog/fcntl.h>
#include <frog/math.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include "file.h"

#include "./ffs_utils.h"
#include "super_block.h"

static int read_inode_zones(struct inode *inode,
                            uint_32 zone_start,
                            uint_32 zone_count,
                            uint_32 *all_zones);


/* // All files opening at same time on this system */
/* struct file g_file_table[MAX_FILE_OPEN]; */
/* struct lock g_ft_lock; */
/*  */
/*  */
/* uint_32 fd_local2global(uint_32 local_fd) */
/* { */
/*         TCB_t *cur = running_thread(); */
/*         int_32 g_fd = cur->fd_table[local_fd]; */
/*         ASSERT(g_fd >= 0 && g_fd < MAX_FILE_OPEN); */
/*         return g_fd; */
/* } */
/*  */
/* struct file *get_file(uint_32 local_fd) */
/* { */
/*         uint_32 gfd = fd_local2global(local_fd); */
/*         return &g_file_table[gfd]; */
/* } */
/*  */
/* #<{(|* */
/*  * Occupy a slot from g_file_table */
/*  * */
/*  * @param void */
/*  * @return return -1 when there is no free slot left. */
/*  ****************************************************************************|)}>#
 */
/* int_32 occupy_file_table_slot(void) */
/* { */
/*         int_32 idx = 0; */
/*         while (idx < MAX_FILE_OPEN) { */
/*                 if (g_file_table[idx].fd_inode == NULL) { */
/*                         break; */
/*                 } */
/*                 idx++; */
/*         } */
/*         if (idx == MAX_FILE_OPEN) { */
/*                 return -1; */
/*         } */
/*         return idx; */
/* } */
/*  */
/* #<{(|* */
/*  * Install given global index to current thread's fd_table[]; */
/*  * */
/*  * @param  fd global file descriptor index */
/*  * @return return idx when success */
/*  *         return -1 when failed */
/*  ****************************************************************************|)}>#
 */
/* int_32 install_thread_fd(int_32 fd) */
/* { */
/*         TCB_t *cur = running_thread(); */
/*         uint_8 fd_idx = 3;  // pass stdin, stdout, stderr */
/*         while (fd_idx < MAX_FILES_OPEN_PER_PROC) { */
/*                 if (cur->fd_table[fd_idx] == -1) {  // if -1 represent free
 * slot */
/*                         cur->fd_table[fd_idx] = fd; */
/*                         break; */
/*                 } */
/*                 fd_idx++; */
/*         } */
/*         if (fd_idx == MAX_FILES_OPEN_PER_PROC) { */
/*                 // TODO: kprint() */
/*                 return -1; */
/*         } */
/*         return fd_idx; */
/* } */
/*  */
/*  */
/* #<{(|* */
/*  * Create file */
/*  * */
/*  * 1. Need new inode aka create a inode (inode_open()) */
/*  *  1.1 inode_nr = inode_bitmap_alloc() */
/*  *      inode = inode_new(inode_nr); */
/*  * */
/*  * 2. Create dir_entry of this file name */
/*  * */
/*  * 3.get file slot form global file_table */
/*  * */
/*  * 4. Add dir_entry to parent directory */
/*  * */
/*  * 5. flush new inode to disk */
/*  *  5.1 flush_inode(inode); */
/*  * */
/*  * 6. flush new dir_entry to disk */
/*  * */
/*  * @param d create file at directory */
/*  * @param name file name */
/*  * @param file option flag */
/*  * @return inode number if success */
/*  *         -1           if failed */
/*  ****************************************************************************|)}>#
 */
/* int_32 file_create(struct partition *part, */
/*                    struct dir *parent_d, */
/*                    char *name, */
/*                    uint_32 flag) */
/* { */
/*         uint_8 rollback_step = 0; */
/*         char *buf = sys_malloc(1024); */
/*         if (!buf) { */
/*                 //  kprint("Not enough memory for io buf"); */
/*                 return -1; */
/*         } */
/*  */
/*         // 1. Need new inode aka create a inode (inode_open()) */
/*         uint_32 inode_nr = inode_bitmap_alloc(part); */
/*         if (inode_nr == -1) { */
/*                 // TODO: */
/*                 //  kprint("Not enough inode bitmap position."); */
/*                 return -1; */
/*         } */
/*         ASSERT(inode_nr != -1); */
/*         struct inode *new_f_inode = sys_malloc(sizeof(struct inode)); */
/*         if (!new_f_inode) { */
/*                 // TODO: */
/*                 //  kprint("Not enough memory for inode ."); */
/*                 // Recover! Need recover inode bitmap set */
/*                 rollback_step = 1; */
/*                 goto roll_back; */
/*         } */
/*         new_inode(inode_nr, new_f_inode); */
/*  */
/*         new_f_inode->i_mode = FT_REGULAR << 11; */
/*         // 2. new dir_entry */
/*         struct dir_entry new_entry; */
/*         new_dir_entry(name, inode_nr, FT_REGULAR, &new_entry); */
/*         // 3.get file slot form global file_table */
/*         lock_fetch(&g_ft_lock); */
/*         uint_32 fd_idx = occupy_file_table_slot(); */
/*         if (fd_idx == -1) { */
/*                 // TODO: */
/*                 //  kprint("Not enough slot at file table."); */
/*                 // Recover! Need recover inode bitmap set */
/*                 //        ! free new_f_inode */
/*                 rollback_step = 2; */
/*                 goto roll_back; */
/*         } */
/*  */
/*         g_file_table[fd_idx].fd_pos = 0; */
/*         g_file_table[fd_idx].fd_flag = flag; */
/*         g_file_table[fd_idx].fd_inode = new_f_inode; */
/*         g_file_table[fd_idx].fd_inode->i_lock = false; */
/*         lock_release(&g_ft_lock); */
/*  */
/*         // 4. flush dir_entry to parents directory */
/*         if (flush_dir_entry(part, parent_d, &new_entry, buf)) { */
/*                 // TODO: */
/*                 //  kprint("Failed at flush directory entry"); */
/*                 rollback_step = 3; */
/*                 // Recover! Need recover inode bitmap set */
/*                 //        ! free new_f_inode */
/*                 //        ! clear g_file_table[fd_idx] */
/*                 goto roll_back; */
/*         } */
/*         // 5. flush parent inode */
/*         //  flush_dir_entry will change parent directory inode size */
/*         memset(buf, 0, 1024);  // 1024 for 2 sectors */
/*         flush_inode(part, parent_d->inode, buf); */
/*         // 6. flush new inode */
/*         memset(buf, 0, 1024);  // 1024 for 2 sectors */
/*         flush_inode(part, new_f_inode, buf); */
/*  */
/*         // 7. flush inode bitmap and zones bitmap */
/*         flush_bitmap(part, INODE_BITMAP, inode_nr); */
/*  */
/*         // 8. add new inode to open_inodes */
/*         list_add(&new_f_inode->inode_tag, &part->open_inodes); */
/*  */
/*         sys_free(buf); */
/*         // 9. install new file file descriptor to current thread. */
/*         return install_thread_fd(fd_idx); */
/*  */
/* roll_back: */
/*         switch (rollback_step) { */
/*         case 3: */
/*                 memset(&g_file_table[fd_idx], 0, sizeof(struct file)); */
/*         case 2: */
/*                 sys_free(new_f_inode); */
/*         case 1: */
/*                 set_value_bitmap(&part->inode_bitmap, inode_nr, 0); */
/*                 break; */
/*         } */
/*         sys_free(buf); */
/*         return -1; */
/* } */
/*  */
/* #<{(|* */
/*  * open a file when it is already created */
/*  * */
/*  * @param inode_nr inode number */
/*  * @param flags file flags */
/*  * @return a file descriptor number */
/*  *         return -1 when failed */
/*  ****************************************************************************|)}>#
 */
/* int_32 file_open(struct partition *part, uint_32 inode_nr, uint_8 flags) */
/* { */
/*         // 1. get slot from global file table */
/*         lock_fetch(&g_ft_lock); */
/*         int_32 gidx = occupy_file_table_slot(); */
/*         if (gidx == -1) { */
/*                 // TODO: */
/*                 // kprint("Not enough global file table slots. when open
 * file"); */
/*                 return -1; */
/*         } */
/*         g_file_table[gidx].fd_inode = inode_open(part, inode_nr); */
/*         g_file_table[gidx].fd_pos = 0; */
/*         g_file_table[gidx].fd_flag = flags; */
/*         lock_release(&g_ft_lock); */
/*  */
/*         bool *write_lock = (bool *) &g_file_table[gidx].fd_inode->i_lock; */
/*         if (flags & O_WRONLY || flags & O_RDWR) { */
/*                 unsigned long flags_status; */
/*                 local_irq_save(flags_status); */
/*                 if (!(*write_lock)) { */
/*                         *write_lock = true; */
/*                         local_irq_restore(flags_status); */
/*                 } else { */
/*                         local_irq_restore(flags_status); */
/*                         // TODO: */
/*                         // kprint("file can not be write now!"); */
/*                         return -1; */
/*                 } */
/*         } */
/*         return install_thread_fd(gidx); */
/* } */
/*  */
/* #<{(|* */
/*  * close a file */
/*  * */
/*  * @param file */
/*  * @return -1 when failed */
/*  *          0 when success */
/*  ****************************************************************************|)}>#
 */
/* int_32 file_close(struct file *file) */
/* { */
/*         if (file == NULL) { */
/*                 return -1; */
/*         } */
/*         file->fd_inode->i_lock = false; */
/*         inode_close(file->fd_inode); */
/*         file->fd_inode = NULL; */
/*         return 0; */
/* } */
/*  */
/* static int_32 rewrite_zone(struct partition *part, */
/*                            struct file *file, */
/*                            uint_32 chunk_rest, */
/*                            uint_32 zone_lba, */
/*                            bool is_last_turn, */
/*                            uint_8 **w_cursor, */
/*                            uint_32 *bytes_written) */
/* { */
/*         // If there is a rest chunk and this ture is the last turn */
/*         if (chunk_rest > 0 && is_last_turn) { */
/*                 // data length == chunk_rest */
/*                 char *rd_io_buf = sys_malloc(512); */
/*                 if (!rd_io_buf) { */
/*                         return -1; */
/*                 } */
/*                 ide_read(part->my_disk, zone_lba, rd_io_buf, 1); */
/*                 memcpy(rd_io_buf, *w_cursor, chunk_rest); */
/*                 ide_write(part->my_disk, zone_lba, rd_io_buf, 1); */
/*                 file->fd_pos += chunk_rest; */
/*                 *bytes_written += chunk_rest; */
/*                 *w_cursor += chunk_rest; */
/*                 sys_free(rd_io_buf); */
/*         } else {  // ZONE_SIZE for 1 sector / 1 zone */
/*                 ide_write(part->my_disk, zone_lba, w_cursor, 1); */
/*                 #<{(| f_inode->i_size += ZONE_SIZE; |)}># */
/*                 file->fd_pos += ZONE_SIZE; */
/*                 *bytes_written += ZONE_SIZE; */
/*                 *w_cursor += ZONE_SIZE; */
/*         } */
/*         return 0; */
/* } */
/*  */
/* static int_32 write_zone(struct partition *part, */
/*                          struct file *file, */
/*                          uint_32 chunk_rest, */
/*                          uint_32 zone_lba, */
/*                          bool is_last_turn, */
/*                          uint_8 **w_cursor, */
/*                          uint_32 *bytes_written) */
/* { */
/*         // If there is a rest chunk and this ture is the last turn */
/*         if (chunk_rest > 0 && is_last_turn) { */
/*                 // data length == chunk_rest */
/*                 uint_8 *tmp = sys_malloc(512); */
/*                 if (!tmp) { */
/*                         // TODO: */
/*                         // kprint("No enough memory when write a file"); */
/*                         return -1; */
/*                 } */
/*                 memcpy(tmp, *w_cursor, chunk_rest); */
/*                 ide_write(part->my_disk, zone_lba, tmp, 1); */
/*                 file->fd_pos += chunk_rest; */
/*                 *bytes_written += chunk_rest; */
/*                 *w_cursor += chunk_rest; */
/*                 sys_free(tmp); */
/*         } else {  // ZONE_SIZE for 1 sector / 1 zone */
/*                 ide_write(part->my_disk, zone_lba, *w_cursor, 1); */
/*                 #<{(| f_inode->i_size += ZONE_SIZE; |)}># */
/*                 file->fd_pos += ZONE_SIZE; */
/*                 *bytes_written += ZONE_SIZE; */
/*                 *w_cursor += ZONE_SIZE; */
/*         } */
/*         return 0; */
/* } */
/*  */
/* static void inode_all_zones(struct partition *part, */
/*                             struct inode *inode, */
/*                             uint_32 *all_zones) */
/* { */
/*         // Find first accessible i_zones[i] */
/*         // 1. read all i_zones; */
/*         // First 12 i_zones[] elements is direct address of data (aka
 * dir_entry) */
/*         for (int i = 0; i < 12 && inode->i_zones[i]; i++) { */
/*                 all_zones[i] = inode->i_zones[i]; */
/*         } */
/*         // the 13th i_zones[] is a in-direct table which size is ZONE_SIZE
 * bytes */
/*         if (inode->i_zones[12]) { */
/*                 ide_read(part->my_disk, inode->i_zones[12], all_zones + 12,
 * 1); */
/*         } */
/* } */

/**
 * write buffer to file
 *
 * @param part mounted partition
 * @param file target file
 * @param buf  buffer contain data
 * @param len writing size
 *
 * @return if success return bytes counts
 *         if failed return -1
 *****************************************************************************/

int_32 write_file(struct super_block *sb,
                  struct file *file,
                  const void *buf,
                  uint_32 write_len)
{
        if (!(file->f_pos < (MAX_FILE_SIZE + 1) && file->f_pos >= 0)) {
                return -1;
        }
        uint_32 count = write_len;
        if ((file->f_pos + count) > MAX_FILE_SIZE) {
                // EOF
                if (file->f_pos < (MAX_FILE_SIZE + 1) && file->f_pos >= 0) {
                        count = MAX_FILE_SIZE - file->f_pos;
                } else {
                        DEBUG("EOF when write");
                        return -1;
                }
        }
        uint_8 *io_buf = kmalloc(ZONE_SIZE);
        if (!io_buf) {
                DEBUG("write file error when no enough memory for io_buf");
                return -1;
        }

        uint_32 bytes_written = 0;
        // record write cursor
        uint_8 *w_cursor = (uint_8 *) buf;
        // this time needs.
        struct inode *f_inode = file->f_inode;
        // start write
        uint_32 write_start = file->f_pos;
        uint_32 write_end = file->f_pos + write_len;
        uint_32 file_end = f_inode->i_size;
        uint_32 allocated_block_count =
            CEIL(f_inode->i_size - file->f_pos, ZONE_SIZE);

        uint_32 first_block = write_start / ZONE_SIZE;
        uint_32 last_block = (write_end - 1) / ZONE_SIZE;
        uint_32 needed_block_count = last_block - first_block + 1;

        uint_32 *zones = kmalloc(4 * needed_block_count);
        read_inode_zones(f_inode, first_block, needed_block_count, zones);

        if (write_start < EOF(file)) {
                // fill first block
                uint_8 *io_buf = kmalloc(ZONE_SIZE);
                uint_32 blk_off_start = write_start % ZONE_SIZE;
                uint_32 blk_rst_sz = ZONE_SIZE - (write_start % ZONE_SIZE);
                read_blocks(sb, first_block, 1, io_buf);
                memcpy(&io_buf[blk_off_start], w_cursor, blk_rst_sz);
                write_blocks(sb, first_block, 1, io_buf);
                w_cursor += blk_rst_sz;
                bytes_written += blk_rst_sz;
                // fill rest blocks
                // how many rest blocks we have
                uint_32 fill_count;
                if (needed_block_count > allocated_block_count) {
                        uint_32 last_blk_idx;
                        fill_count = allocated_block_count;
                        for (int i = 1; i < fill_count; i++) {
                                memset(io_buf, 0, ZONE_SIZE);
                                memcpy(io_buf, w_cursor, ZONE_SIZE);
                                write_blocks(sb, zones[i], 1, io_buf);
                                last_blk_idx = zones[i];
                                w_cursor += ZONE_SIZE;
                                bytes_written += ZONE_SIZE;
                        }
                        uint_32 rst_blk_count =
                            needed_block_count - allocated_block_count;
                        // need new zone index
                        for (; rst_blk_count < 1;) {
                                uint_32 new_zone_idx = alloc_zone_bitmap(sb);
                                flush_bitmap_block(sb, ZONE_BITMAP,
                                                   new_zone_idx);

                                memset(io_buf, 0, ZONE_SIZE);
                                memcpy(io_buf, w_cursor, ZONE_SIZE);
                                write_blocks(sb, new_zone_idx, 1, io_buf);
                                w_cursor += ZONE_SIZE;
                                bytes_written += ZONE_SIZE;
                                rst_blk_count--;
                        }
                        // last block
                        uint_32 rst_len = write_len - bytes_written;
                        uint_32 zone_idx = alloc_zone_bitmap(sb);
                        flush_bitmap_block(sb, ZONE_BITMAP, zone_idx);
                        memset(io_buf, 0, ZONE_SIZE);
                        memcpy(io_buf, w_cursor, rst_len);
                        write_blocks(sb, zone_idx, 1, io_buf);
                        w_cursor += rst_len;
                        bytes_written += rst_len;
                } else {
                        fill_count = needed_block_count;
                }
        } else {

        }

        kfree(zones);
        kfree(io_buf);

        return bytes_written;
}

/**
 * read file to buffer
 *
 * read()  attempts  to  read up to count bytes from file
 * descriptor fd into the buffer starting at buf.
 *
 * On files that support seeking, the read operation com‐
 * mences  at the file offset, and the file offset is in‐
 * cremented by the number of bytes read.   If  the  file
 * offset  is  at  or  past the end of file, no bytes are
 * read, and read() returns zero.
 *
 * If count is zero, read() may  detect  the  errors  de‐
 * scribed  below.   In  the absence of any errors, or if
 * read() does not check for  errors,  a  read()  with  a
 * count of 0 returns zero and has no other effects.
 *
 * According   to  POSIX.1,  if  count  is  greater  than
 * SSIZE_MAX, the result is  implementation-defined;  see
 * NOTES for the upper limit on Linux.
 *
 * @param part mounted partition
 * @param file target file
 * @param buf  buffer contain data
 * @param count reading size
 *
 * @return if success return bytes counts
 *         if failed return -1
 *****************************************************************************/
static int read_inode_zones(struct inode *inode,
                            uint_32 zone_start,
                            uint_32 zone_count,
                            uint_32 *all_zones)
{
        all_zones[0] = zone_start;
        for (int i = 1; i < zone_count; i++) {
                int_32 next_zone_blk = next_inode_zone_blk(inode, zone_start);
                all_zones[i] = next_zone_blk;
        }
        return 0;
}

int_32 read_file(struct super_block *sb,
                 struct file *file,
                 void *buf,
                 uint_32 count)
{
        uint_8 *io_buf = kmalloc(ZONE_SIZE);
        if (!io_buf) {
                // TODO
                // kprint("Not enough memory when file_read()");
                return -1;
        }
        ASSERT(file->f_pos >= 0 && file->f_pos <= EOF(file));
        if (file->f_pos < 0 || file->f_pos >= EOF(file)) {
                // TODO
                // kprint("Wrong file position.");
                kfree(io_buf);
                return -1;
        }
        // test file->fd_pos at the end
        // read all file
        uint_32 file_pos = file->f_pos;
        uint_32 f_left_sz = file->f_inode->i_size - file->f_pos;

        // 2.This time Read file range
        // Read data from file_pos, and read data is rd_len
        uint_32 rd_len = MIN(f_left_sz, count);

        // Find first accessible i_zones[i]
        // 1. read all i_zones;
        uint_32 zone_start = file_pos / ZONE_SIZE;
        uint_32 zone_count = CEIL(rd_len, ZONE_SIZE);
        uint_32 *all_zones = kmalloc(zone_count * sizeof(uint_32));

        read_inode_zones(file->f_inode, zone_start, zone_count, all_zones);

        // 3. Covert file_pos to r_lba
        uint_32 rd_zone_offset = file_pos % ZONE_SIZE;
        uint_32 r_start = all_zones[0];

        if (rd_len < ZONE_SIZE) {
                read_blocks(sb, r_start, 1, io_buf);
                memcpy(buf, &io_buf[rd_zone_offset], rd_len);
                file->f_pos += rd_len;
                buf += rd_len;
                kfree(io_buf);
                kfree(all_zones);
                return rd_len;
        } else {  // read len is large than 1 zone size
                uint_32 rd_count = CEIL(rd_len, ZONE_SIZE);
                uint_32 rd_count_offset = rd_len % ZONE_SIZE;
                uint_32 b_idx;
                for (int i = 0; i < rd_count; i++) {
                        b_idx = all_zones[i];
                        read_blocks(sb, b_idx, 1, io_buf);

                        if (rd_zone_offset > 0) {
                                int_32 first_len = ZONE_SIZE - rd_zone_offset;
                                memcpy(buf, &io_buf[rd_zone_offset], first_len);
                                file->f_pos += first_len;
                                buf += first_len;
                                rd_zone_offset = 0;
                                continue;
                        }

                        if ((i == (rd_count - 1)) && rd_count_offset != 0) {
                                memcpy(buf, io_buf, rd_count_offset);
                                file->f_pos += rd_count_offset;
                                buf += rd_count_offset;
                        } else {
                                memcpy(buf, io_buf, ZONE_SIZE);
                                file->f_pos += ZONE_SIZE;
                                buf += ZONE_SIZE;
                        }
                }
                kfree(io_buf);
                kfree(all_zones);
                return rd_len;
        }
}
