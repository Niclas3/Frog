#include "frogfs.h"
#include <frog/block.h>
#include <frog/compiler.h>
#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/mount.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include "ffs_utils.h"

#include "inode.h"
#include "super_block.h"

static struct super_block *frogfs_mount(struct fs_type *fs,
                                        int flags,
                                        const char *dev,
                                        void *data);

static struct fs_type frogfs_type = {.name = "frogfs", .mount = frogfs_mount};
static struct super_operations frog_sop = {
    .put_super = frogfs_put_super,
    .statfs = frogfs_statfs,
    .remount = frogfs_remount,
    .alloc_inode = frogfs_alloc_inode,
    .destory_inode = frogfs_destory_inode,
    .write_inode = frogfs_write_inode,
    .sync_fs = frogfs_sync_fs,
    .evict_inode = frogfs_evict_inode,
};

static struct file_operations frog_fop = {.open = frogfs_open,
                                          .close = frogfs_close,
                                          .read = frogfs_read,
                                          .write = frogfs_write,
                                          .lseek = frogfs_lseek,
                                          .mmap = frogfs_mmap};

static struct inode_operations frog_iop = {
    .create = frogfs_create,
    .mkdir = frogfs_mkdir,
    .rmdir = frogfs_rmdir,
    .unlink = frogfs_unlink,
    .lookup = frogfs_lookup,
    .rename = frogfs_rename,
    .link = frogfs_link,
    .symlink = frogfs_symlink,
};


static struct dentry_operations frog_dop = {};

static struct address_space_operations frog_asop = {};

#define FROGFS_DIRECT_ZONE_COUNT 11
#define FROGFS_INDIRECT_TABLE_COUNT (ZONE_IDX_MAX - FROGFS_DIRECT_ZONE_COUNT)
#define FROGFS_INDIRECT_ENTRY_COUNT (ZONE_SIZE / sizeof(uint_32))
#define FROGFS_DIRENT_NAME_MAX 16

struct frogfs_dir_entry {
        char filename[FROGFS_DIRENT_NAME_MAX];
        uint_32 i_no;
        enum file_type f_type;
};

static void frogfs_fill_vfs_inode(struct inode *inode,
                                  struct frogfs_inode *finode)
{
        inode->i_num = finode->i_num;
        inode->i_mode = finode->i_mode;
        inode->i_size = finode->i_size;
        inode->i_nlink = finode->i_nlinks;
        inode->i_uid = finode->i_uid;
        inode->i_gid = finode->i_gid;
        inode->i_atime = finode->i_atime;
        inode->i_ctime = finode->i_ctime;
        inode->i_mtime = finode->i_mtime;
        inode->i_dev = finode->i_dev;
}

static int_32 frogfs_alloc_zone_block(struct super_block *sb)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        int_32 zone_idx = alloc_zone_bitmap(sb);
        if (zone_idx < 0)
                return -ENOSPC;

        flush_bitmap_block(sb, ZONE_BITMAP, zone_idx);
        return fsb->disk_sb.s_data_start_blk + zone_idx;
}

static int_32 frogfs_zero_zone(struct super_block *sb, uint_32 block_no)
{
        uint_8 *buf = kmalloc(ZONE_SIZE);
        if (!buf)
                return -ENOMEM;

        memset(buf, 0, ZONE_SIZE);
        int ret = write_blocks(sb, block_no, 1, buf);
        kfree(buf);

        return ret < 0 ? -EIO : 0;
}

static int_32 frogfs_get_file_block(struct inode *inode,
                                    uint_32 file_block_idx,
                                    bool create)
{
        if (!inode || !inode->i_sb || !inode->i_private)
                return -EINVAL;
        if (file_block_idx >= MAX_ZONE_COUNT)
                return -EFBIG;

        struct frogfs_inode *finode = inode->i_private;
        struct super_block *sb = inode->i_sb;

        if (file_block_idx < FROGFS_DIRECT_ZONE_COUNT) {
                if (!finode->i_zones[file_block_idx] && create) {
                        int_32 block_no = frogfs_alloc_zone_block(sb);
                        if (block_no < 0)
                                return block_no;
                        finode->i_zones[file_block_idx] = block_no;
                        finode->i_blocks++;
                }
                return finode->i_zones[file_block_idx];
        }

        uint_32 indirect_idx =
            (file_block_idx - FROGFS_DIRECT_ZONE_COUNT) /
            FROGFS_INDIRECT_ENTRY_COUNT;
        uint_32 entry_idx =
            (file_block_idx - FROGFS_DIRECT_ZONE_COUNT) %
            FROGFS_INDIRECT_ENTRY_COUNT;

        if (indirect_idx >= FROGFS_INDIRECT_TABLE_COUNT)
                return -EFBIG;

        uint_32 table_slot = FROGFS_DIRECT_ZONE_COUNT + indirect_idx;
        if (!finode->i_zones[table_slot]) {
                if (!create)
                        return 0;

                int_32 table_block = frogfs_alloc_zone_block(sb);
                if (table_block < 0)
                        return table_block;

                finode->i_zones[table_slot] = table_block;
                finode->i_blocks++;
                int ret = frogfs_zero_zone(sb, table_block);
                if (ret < 0)
                        return ret;
        }

        uint_32 *entries = kmalloc(ZONE_SIZE);
        if (!entries)
                return -ENOMEM;

        if (read_blocks(sb, finode->i_zones[table_slot], 1,
                        (uint_8 *) entries) < 0) {
                kfree(entries);
                return -EIO;
        }

        if (!entries[entry_idx] && create) {
                int_32 block_no = frogfs_alloc_zone_block(sb);
                if (block_no < 0) {
                        kfree(entries);
                        return block_no;
                }

                entries[entry_idx] = block_no;
                finode->i_blocks++;
                if (write_blocks(sb, finode->i_zones[table_slot], 1,
                                 (uint_8 *) entries) < 0) {
                        kfree(entries);
                        return -EIO;
                }
        }

        int_32 block_no = entries[entry_idx];
        kfree(entries);
        return block_no;
}

static uint_32 frogfs_dir_entry_size(struct super_block *sb)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        uint_32 entry_size = fsb->disk_sb.dir_entry_size;

        if (entry_size == 0)
                entry_size = sizeof(struct frogfs_dir_entry);
        return entry_size;
}

static bool frogfs_valid_dir_entry_size(uint_32 entry_size)
{
        return entry_size >= sizeof(struct frogfs_dir_entry) &&
               entry_size <= ZONE_SIZE;
}

static bool frogfs_dir_entry_is_empty(struct frogfs_dir_entry *entry)
{
        return entry->filename[0] == '\0' && entry->i_no == 0 &&
               entry->f_type == FT_UNKOWN;
}

static bool frogfs_dir_entry_name_eq(struct frogfs_dir_entry *entry,
                                     const char *name)
{
        return strncmp(entry->filename, name, FROGFS_DIRENT_NAME_MAX) == 0;
}

static void frogfs_init_dir_entry(struct frogfs_dir_entry *entry,
                                  const char *name,
                                  uint_32 inode_no,
                                  enum file_type type)
{
        memset(entry, 0, sizeof(struct frogfs_dir_entry));
        strncpy(entry->filename, name, FROGFS_DIRENT_NAME_MAX - 1);
        entry->i_no = inode_no;
        entry->f_type = type;
}

static int_32 frogfs_lookup_dir_entry(struct inode *dir, const char *name)
{
        struct frogfs_inode *finode = dir->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        if (!frogfs_valid_dir_entry_size(entry_size))
                return -EIO;

        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = DIV_ROUND_UP(finode->i_size, entry_size);
        uint_8 *buf = kmalloc(ZONE_SIZE);
        if (!buf)
                return -ENOMEM;

        for (uint_32 entry_idx = 0; entry_idx < entry_count; entry_idx++) {
                uint_32 file_block_idx = entry_idx / entries_per_zone;
                uint_32 block_entry_idx = entry_idx % entries_per_zone;
                uint_32 block_offset = block_entry_idx * entry_size;

                int_32 block_no =
                    frogfs_get_file_block(dir, file_block_idx, false);
                if (block_no < 0) {
                        kfree(buf);
                        return block_no;
                }
                if (block_no == 0)
                        continue;

                if (read_blocks(dir->i_sb, block_no, 1, buf) < 0) {
                        kfree(buf);
                        return -EIO;
                }

                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *) (buf + block_offset);
                if (!frogfs_dir_entry_is_empty(entry) &&
                    frogfs_dir_entry_name_eq(entry, name)) {
                        int_32 inode_no = entry->i_no;
                        kfree(buf);
                        return inode_no;
                }
        }

        kfree(buf);
        return -ENOENT;
}

static int_32 frogfs_add_dir_entry(struct inode *dir,
                                   struct frogfs_dir_entry *new_entry,
                                   void *io_buf)
{
        struct frogfs_inode *finode = dir->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        if (!frogfs_valid_dir_entry_size(entry_size))
                return -EIO;

        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = DIV_ROUND_UP(finode->i_size, entry_size);
        uint_8 *buf = io_buf;

        for (uint_32 entry_idx = 0; entry_idx <= entry_count; entry_idx++) {
                uint_32 file_block_idx = entry_idx / entries_per_zone;
                uint_32 block_entry_idx = entry_idx % entries_per_zone;
                uint_32 block_offset = block_entry_idx * entry_size;
                bool appending = entry_idx == entry_count;

                int_32 block_no =
                    frogfs_get_file_block(dir, file_block_idx, false);
                if (block_no < 0)
                        return block_no;

                if (block_no == 0) {
                        block_no =
                            frogfs_get_file_block(dir, file_block_idx, true);
                        if (block_no < 0)
                                return block_no;
                        memset(buf, 0, ZONE_SIZE);
                } else {
                        if (read_blocks(dir->i_sb, block_no, 1, buf) < 0)
                                return -EIO;
                }

                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *) (buf + block_offset);
                if (!appending && !frogfs_dir_entry_is_empty(entry))
                        continue;

                memset(entry, 0, entry_size);
                memcpy(entry, new_entry, sizeof(struct frogfs_dir_entry));
                if (write_blocks(dir->i_sb, block_no, 1, buf) < 0)
                        return -EIO;

                if (appending)
                        finode->i_size += entry_size;
                frogfs_fill_vfs_inode(dir, finode);
                dir->i_dirty = true;
                return 0;
        }

        return -ENOSPC;
}

static int_32 frogfs_remove_dir_entry(struct inode *dir,
                                       const char *name,
                                       void *io_buf)
{
        struct frogfs_inode *finode = dir->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        if (!frogfs_valid_dir_entry_size(entry_size))
                return -EIO;

        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = DIV_ROUND_UP(finode->i_size, entry_size);
        uint_8 *buf = io_buf;

        for (uint_32 entry_idx = 0; entry_idx < entry_count; entry_idx++) {
                uint_32 file_block_idx = entry_idx / entries_per_zone;
                uint_32 block_entry_idx = entry_idx % entries_per_zone;
                uint_32 block_offset = block_entry_idx * entry_size;

                int_32 block_no =
                    frogfs_get_file_block(dir, file_block_idx, false);
                if (block_no <= 0)
                        continue;

                if (read_blocks(dir->i_sb, block_no, 1, buf) < 0)
                        return -EIO;

                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *) (buf + block_offset);
                if (frogfs_dir_entry_is_empty(entry))
                        continue;
                if (!frogfs_dir_entry_name_eq(entry, name))
                        continue;

                memset(entry, 0, entry_size);
                if (write_blocks(dir->i_sb, block_no, 1, buf) < 0)
                        return -EIO;
                return 0;
        }

        return -ENOENT;
}

static uint_16 frogfs_regular_mode(uint_32 mode)
{
        uint_32 file_type = GET_FILE_TYPE(mode);
        uint_32 permissions = mode & 0777;

        if (file_type == FT_UNKOWN) {
                if (mode <= FT_REGULAR) {
                        file_type = mode;
                        permissions = 0;
                } else {
                        file_type = FT_REGULAR;
                }
        }

        return (uint_16) ((file_type << 11) | permissions);
}

static uint_16 frogfs_directory_mode(uint_32 mode)
{
        uint_32 permissions = mode > FT_REGULAR ? mode & 0777 : 0;

        return (uint_16) ((FT_DIRECTORY << 11) | permissions);
}

static void frogfs_free_zone_block(struct super_block *sb, uint_32 block_no)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;

        if (block_no < fsb->disk_sb.s_data_start_blk)
                return;

        int_32 zone_idx = block_no - fsb->disk_sb.s_data_start_blk;
        free_znode_bitmap(sb, zone_idx);
        flush_bitmap_block(sb, ZONE_BITMAP, zone_idx);
}


void frogfs_put_super(struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return;
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        write_bitmap(sb, fsb->disk_sb.s_zmap_blk, fsb->disk_sb.s_zmap_sz,
                     fsb->z_bmap);
        write_bitmap(sb, fsb->disk_sb.s_imap_blk, fsb->disk_sb.s_imap_sz,
                     fsb->i_bmap);
        kfree(fsb);
        sb->s_fs_info = NULL;
}

int_32 frogfs_statfs(struct super_block *sb, struct stat *buf)
{
        return 0;
}

int_32 frogfs_remount(struct super_block *sb, uint_32 flags)
{
        return 0;
}

struct inode *frogfs_alloc_inode(struct super_block *sb)
{
        int_32 inum = alloc_inode_bitmap(sb);
        if (inum == -1) {
                DEBUG("not enough inode number");
                return NULL;
        }

        struct inode *inode = kmalloc(sizeof(struct inode));
        if (!inode) {
                free_inode_bitmap(sb, inum);
                DEBUG("not enough memory");
                return NULL;
        }

        struct frogfs_inode *finode = kmalloc(sizeof(struct frogfs_inode));
        if (!finode) {
                free_inode_bitmap(sb, inum);
                kfree(inode);
                DEBUG("not enough memory");
                return NULL;
        }

        memset(inode, 0, sizeof(struct inode));
        memset(finode, 0, sizeof(struct frogfs_inode));

        finode->i_num = inum;
        inode->i_num = inum;
        inode->i_sb = sb;
        inode->i_op = &frog_iop;
        inode->i_fop = &frog_fop;
        inode->i_private = finode;
        INIT_LIST_HEAD(&inode->i_active_node);

        return inode;
}

void frogfs_destory_inode(struct super_block *sb, struct inode *target)
{
        // free all resource of inode
        // mark all zone in this inode as free
        ASSERT(target);
        if (!target) {
                return;
        }

        uint_32 inum = target->i_num;

        if (target->i_private) {
                kfree(target->i_private);
        }

        kfree(target);

        free_inode_bitmap(sb, inum);

        return;
}

void frogfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
        (void) wbc;
        if (!inode || !inode->i_dirty || !inode->i_sb)
                return;
        uint_8 *buf = kmalloc(2 * ZONE_SIZE);
        if (!buf)
                return;
        flush_inode(inode->i_sb, inode, buf);
        inode->i_dirty = false;
        kfree(buf);
}

void frogfs_evict_inode(struct inode *inode)
{
        if (!inode || !inode->i_sb)
                return;
        if (inode->i_dirty) {
                uint_8 *buf = kmalloc(2 * ZONE_SIZE);
                if (buf) {
                        flush_inode(inode->i_sb, inode, buf);
                        kfree(buf);
                        inode->i_dirty = false;
                }
        }
        list_del(&inode->i_active_node);
}

int frogfs_sync_fs(struct super_block *sb, int wait)
{
        // flush zone bitmap
        // flush inode bitmap
        // flush all dirty inodes
        // flush all dirty dir blocks
        // flush super blocks
        return 0;
}

static struct inode *frogfs_create_root_inode(struct block_device *bdev,
                                              struct super_block *sb)
{
        (void) bdev;

        struct inode *root_inode = kmalloc(sizeof(struct inode));
        if (!root_inode) {
                return NULL;
        }
        memset(root_inode, 0, sizeof(struct inode));

        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;

        struct frogfs_inode *inode_table =
            kmalloc(fsb->disk_sb.s_inode_table_sz * ZONE_SIZE);
        if (!inode_table) {
                kfree(root_inode);
                return NULL;
        }

        // read root inode from disk
        int size = read_blocks(sb, fsb->disk_sb.s_inode_table_blk,
                               fsb->disk_sb.s_inode_table_sz,
                               (uint_8 *) inode_table);
        if (!size) {
                kfree(inode_table);
                kfree(root_inode);
                return NULL;
        }
        uint_32 root_inode_no = ((struct frogfs_super_block *) sb->s_fs_info)
                                    ->disk_sb.root_inode_no;
        ASSERT(root_inode_no == 0);
        struct frogfs_inode *finode = &inode_table[root_inode_no];

        root_inode->i_private = finode;
        root_inode->i_op = &frog_iop;
        root_inode->i_fop = &frog_fop;
        root_inode->i_bdop = NULL;
        root_inode->i_sb = sb;
        frogfs_fill_vfs_inode(root_inode, finode);

        return root_inode;
}

static void frogfs_set_bitmap_bit(uint_8 *bits, uint_32 bit_idx)
{
        bits[bit_idx / 8] |= (1 << (bit_idx % 8));
}

static int_32 frogfs_format_partition(struct block_device *bdev,
                                       struct frogfs_super_block *fsb)
{
        if (!bdev || !fsb)
                return -EINVAL;
        if (bdev->bd_start_lba % SECTOR_PER_ZONE)
                return -EINVAL;

        uint_32 start_blk = bdev->bd_start_lba / SECTOR_PER_ZONE;
        uint_32 total_blks = bdev->bd_sec_cnt / SECTOR_PER_ZONE;
        uint_32 super_blk_sz = 1;
        uint_32 inode_bitmap_sz =
            DIV_ROUND_UP(MAX_FILES_PER_PARTITION, ZONE_SIZE);
        uint_32 inode_table_sz =
            DIV_ROUND_UP(sizeof(struct frogfs_inode) *
                             MAX_FILES_PER_PARTITION,
                         ZONE_SIZE);
        uint_32 used_blks_without_zmap =
            super_blk_sz + inode_bitmap_sz + inode_table_sz;

        if (total_blks <= used_blks_without_zmap)
                return -ENOSPC;

        uint_32 free_blks = total_blks - used_blks_without_zmap;
        uint_32 zone_bitmap_sz = DIV_ROUND_UP(free_blks, ZONE_SIZE + 1);
        uint_32 data_zone_count = free_blks - zone_bitmap_sz;
        if (!data_zone_count)
                return -ENOSPC;

        memset(fsb, 0, sizeof(struct frogfs_super_block));
        fsb->disk_sb.s_magic = FROGFS_MAGIC;
        strncpy(fsb->disk_sb.vol_name, "frogfs",
                sizeof(fsb->disk_sb.vol_name) - 1);
        fsb->disk_sb.s_ninodes = MAX_FILES_PER_PARTITION;
        fsb->disk_sb.s_inode_sz = sizeof(struct frogfs_inode);
        fsb->disk_sb.s_nzones = data_zone_count;
        fsb->disk_sb.s_zone_sz = ZONE_SIZE;
        fsb->disk_sb.s_imap_blk = start_blk + super_blk_sz;
        fsb->disk_sb.s_imap_sz = inode_bitmap_sz;
        fsb->disk_sb.s_zmap_blk =
            fsb->disk_sb.s_imap_blk + inode_bitmap_sz;
        fsb->disk_sb.s_zmap_sz = zone_bitmap_sz;
        fsb->disk_sb.s_inode_table_blk =
            fsb->disk_sb.s_zmap_blk + zone_bitmap_sz;
        fsb->disk_sb.s_inode_table_sz = inode_table_sz;
        fsb->disk_sb.s_data_start_blk =
            fsb->disk_sb.s_inode_table_blk + inode_table_sz;
        fsb->disk_sb.root_inode_no = 0;
        fsb->disk_sb.dir_entry_size = sizeof(struct frogfs_dir_entry);
        fsb->disk_sb.s_log_zone_sz = 1;
        fsb->disk_sb.s_max_file_sz = MAX_FILE_SIZE;

        if (bio_write(bdev, bdev->bd_start_lba, &fsb->disk_sb, 1) < 0)
                return -EIO;

        struct super_block format_sb;
        memset(&format_sb, 0, sizeof(format_sb));
        format_sb.s_bdev = bdev;
        format_sb.s_fs_info = fsb;

        uint_8 *buf = kmalloc(ZONE_SIZE);
        if (!buf)
                return -ENOMEM;

        memset(buf, 0, ZONE_SIZE);
        for (uint_32 idx = 0; idx < fsb->disk_sb.s_imap_sz; idx++) {
                if (write_blocks(&format_sb, fsb->disk_sb.s_imap_blk + idx, 1,
                                 buf) < 0) {
                        kfree(buf);
                        return -EIO;
                }
        }

        frogfs_set_bitmap_bit(buf, 0);
        if (write_blocks(&format_sb, fsb->disk_sb.s_imap_blk, 1, buf) < 0) {
                kfree(buf);
                return -EIO;
        }

        for (uint_32 blk_idx = 0; blk_idx < fsb->disk_sb.s_zmap_sz;
             blk_idx++) {
                memset(buf, 0, ZONE_SIZE);

                for (uint_32 bit_idx = 0; bit_idx < BITS_PER_ZONE;
                     bit_idx++) {
                        uint_32 zone_idx = blk_idx * BITS_PER_ZONE + bit_idx;

                        if (zone_idx == 0 ||
                            zone_idx >= fsb->disk_sb.s_nzones) {
                                frogfs_set_bitmap_bit(buf, bit_idx);
                        }
                }

                if (write_blocks(&format_sb,
                                 fsb->disk_sb.s_zmap_blk + blk_idx, 1,
                                 buf) < 0) {
                        kfree(buf);
                        return -EIO;
                }
        }

        memset(buf, 0, ZONE_SIZE);
        for (uint_32 idx = 0; idx < fsb->disk_sb.s_inode_table_sz; idx++) {
                if (write_blocks(&format_sb,
                                 fsb->disk_sb.s_inode_table_blk + idx, 1,
                                 buf) < 0) {
                        kfree(buf);
                        return -EIO;
                }
        }

        struct frogfs_inode *root_inode = (struct frogfs_inode *) buf;
        root_inode->i_num = 0;
        root_inode->i_mode = FT_DIRECTORY << 11;
        root_inode->i_size = fsb->disk_sb.dir_entry_size * 2;
        root_inode->i_nlinks = 2;
        root_inode->i_zones[0] = fsb->disk_sb.s_data_start_blk;
        root_inode->i_blocks = 1;
        if (write_blocks(&format_sb, fsb->disk_sb.s_inode_table_blk, 1, buf) <
            0) {
                kfree(buf);
                return -EIO;
        }

        memset(buf, 0, ZONE_SIZE);
        struct frogfs_dir_entry *dot = (struct frogfs_dir_entry *) buf;
        struct frogfs_dir_entry *dotdot = dot + 1;
        frogfs_init_dir_entry(dot, ".", 0, FT_DIRECTORY);
        frogfs_init_dir_entry(dotdot, "..", 0, FT_DIRECTORY);
        if (write_blocks(&format_sb, fsb->disk_sb.s_data_start_blk, 1, buf) <
            0) {
                kfree(buf);
                return -EIO;
        }

        kfree(buf);
        return 0;
}

static struct super_block *frogfs_mount(struct fs_type *fs,
                                        int flags,
                                        const char *dev,
                                        void *data)
{
        struct super_block *sb = kmalloc(sizeof(struct super_block));
        if (!sb)
                return NULL;

        // /dev/sdb0p1
        struct dentry *bdev = vfs_lookup(dev);
        ASSERT(bdev);
        dev_t bdev_no = bdev->d_inode->i_dev;
        struct block_device *frogfs_bdev = get_block_device(bdev_no);
        if (!frogfs_bdev) {
                INFO("[frogfs]: not find target block device %d", bdev_no);
                PANIC("!");
                return NULL;
        }

        struct frogfs_super_block *fsb =
            kmalloc(sizeof(struct frogfs_super_block));

        // read_super_block_from_disk fill into fsb
        int rd_size = bio_read(frogfs_bdev, frogfs_bdev->bd_start_lba, fsb, 1);
        if (rd_size <= 0) {
                INFO("[frogfs]: read super block error ");
        }
        if (fsb->disk_sb.s_magic != FROGFS_MAGIC) {
                INFO("[frogfs]: super block magic error, format partition");
                int_32 ret = frogfs_format_partition(frogfs_bdev, fsb);
                if (ret < 0) {
                        INFO("[frogfs]: format partition failed %d", ret);
                        PANIC("!");
                }
        }

        int rd = bio_read(frogfs_bdev, frogfs_bdev->bd_start_lba, fsb, 1);
        if (rd <= 0) {
                INFO("[frogfs]:2nd read super block error ");
        }

        sb->s_fs_info = fsb;
        sb->s_devno = bdev_no;
        sb->s_magic = fsb->disk_sb.s_magic;
        sb->s_op = &frog_sop;
        sb->s_block_size = fsb->disk_sb.s_zone_sz;
        sb->s_bdev = frogfs_bdev;

        // read inode bitmap from disk
        struct bitmap *i_bmap = kmalloc(sizeof(struct bitmap));
        int ibmap_start = fsb->disk_sb.s_imap_blk;
        int ibmap_size = fsb->disk_sb.s_imap_sz;
        read_bitmap(sb, ibmap_start, ibmap_size, i_bmap);

        // read zone bitmap from disk
        struct bitmap *z_bmap = kmalloc(sizeof(struct bitmap));
        int zbmap_start = fsb->disk_sb.s_zmap_blk;
        int zbmap_size = fsb->disk_sb.s_zmap_sz;
        read_bitmap(sb, zbmap_start, zbmap_size, z_bmap);

        fsb->i_bmap = i_bmap;
        fsb->z_bmap = z_bmap;

        INIT_LIST_HEAD(&sb->s_inodes);

        struct inode *root_inode = frogfs_create_root_inode(frogfs_bdev, sb);

        list_add_tail(&root_inode->i_active_node, &sb->s_inodes);

        if (!root_inode)
                return NULL;

        sb->s_root = root_inode;
        return sb;
}


int_32 frogfs_open(struct inode *dinode, struct file *file)
{
        if (!dinode || !file)
                return -EINVAL;
        if (!dinode->i_sb || !dinode->i_private)
                return -EINVAL;

        struct frogfs_inode *finode = dinode->i_private;
        uint_32 access_mode = file->f_flag & O_ACCMODE;
        bool writable = access_mode == O_WRONLY || access_mode == O_RDWR;
        uint_32 file_type = GET_FILE_TYPE(finode->i_mode);

        if ((file->f_flag & O_DIRECTORY) && file_type != FT_DIRECTORY)
                return -ENOTDIR;
        if (writable && file_type == FT_DIRECTORY)
                return -EISDIR;

        frogfs_fill_vfs_inode(dinode, finode);
        file->f_inode = dinode;
        file->f_op = dinode->i_fop;
        file->private_data = finode;
        if (file->f_flag & O_APPEND)
                file->f_pos = dinode->i_size;

        unsigned long flags;
        local_irq_save(flags);
        if (writable) {
                if (dinode->i_lock) {
                        local_irq_restore(flags);
                        return -EBUSY;
                }
                dinode->i_lock = true;
        }
        dinode->i_count++;
        local_irq_restore(flags);

        return 0;
}

int_32 frogfs_close(struct file *file)
{
        if (!file || !file->f_inode)
                return -EINVAL;

        struct inode *inode = file->f_inode;
        uint_32 access_mode = file->f_flag & O_ACCMODE;
        bool writable = access_mode == O_WRONLY || access_mode == O_RDWR;

        unsigned long flags;
        local_irq_save(flags);
        if (writable)
                inode->i_lock = false;
        if (inode->i_count > 0)
                inode->i_count--;
        local_irq_restore(flags);

        file->private_data = NULL;
        return 0;
}

int_32 frogfs_read(struct file *file, void *buf, uint_32 count)
{
        if (!file || !file->f_inode || !buf)
                return -EINVAL;
        if ((file->f_flag & O_ACCMODE) == O_WRONLY)
                return -EBADF;
        if (!file->f_inode->i_sb || !file->f_inode->i_private)
                return -EINVAL;

        struct inode *inode = file->f_inode;
        struct frogfs_inode *finode = inode->i_private;
        if (GET_FILE_TYPE(finode->i_mode) == FT_DIRECTORY)
                return -EISDIR;

        if (count == 0)
                return 0;
        if (file->f_pos >= finode->i_size)
                return 0;

        uint_8 *io_buf = kmalloc(ZONE_SIZE);
        if (!io_buf)
                return -ENOMEM;

        uint_8 *read_cursor = buf;
        uint_32 bytes_read = 0;
        uint_32 readable = MIN(count, finode->i_size - file->f_pos);

        while (bytes_read < readable) {
                uint_32 file_pos = file->f_pos + bytes_read;
                uint_32 file_block_idx = file_pos / ZONE_SIZE;
                uint_32 block_offset = file_pos % ZONE_SIZE;
                uint_32 chunk =
                    MIN(ZONE_SIZE - block_offset, readable - bytes_read);

                int_32 block_no =
                    frogfs_get_file_block(inode, file_block_idx, false);
                if (block_no < 0) {
                        kfree(io_buf);
                        return bytes_read ? (int_32) bytes_read : block_no;
                }

                if (block_no == 0) {
                        memset(read_cursor, 0, chunk);
                } else {
                        if (read_blocks(inode->i_sb, block_no, 1, io_buf) <
                            0) {
                                kfree(io_buf);
                                return bytes_read ? (int_32) bytes_read : -EIO;
                        }
                        memcpy(read_cursor, io_buf + block_offset, chunk);
                }

                read_cursor += chunk;
                bytes_read += chunk;
        }

        file->f_pos += bytes_read;
        frogfs_fill_vfs_inode(inode, finode);
        kfree(io_buf);

        return bytes_read;
}

int_32 frogfs_write(struct file *file, const void *buf, uint_32 count)
{
        if (!file || !file->f_inode || !buf)
                return -EINVAL;
        if (!file->f_inode->i_sb || !file->f_inode->i_private)
                return -EINVAL;

        uint_32 access_mode = file->f_flag & O_ACCMODE;
        if (access_mode != O_WRONLY && access_mode != O_RDWR)
                return -EBADF;

        struct inode *inode = file->f_inode;
        struct frogfs_inode *finode = inode->i_private;
        if (GET_FILE_TYPE(finode->i_mode) == FT_DIRECTORY)
                return -EISDIR;

        if (count == 0)
                return 0;
        if (file->f_pos > finode->i_size)
                return -EINVAL;
        if (file->f_pos >= MAX_FILE_SIZE)
                return -EFBIG;

        uint_32 writable = MIN(count, MAX_FILE_SIZE - file->f_pos);
        uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
        if (!io_buf)
                return -ENOMEM;

        const uint_8 *write_cursor = buf;
        uint_32 bytes_written = 0;

        while (bytes_written < writable) {
                uint_32 file_pos = file->f_pos + bytes_written;
                uint_32 file_block_idx = file_pos / ZONE_SIZE;
                uint_32 block_offset = file_pos % ZONE_SIZE;
                uint_32 chunk =
                    MIN(ZONE_SIZE - block_offset, writable - bytes_written);

                int_32 block_no =
                    frogfs_get_file_block(inode, file_block_idx, true);
                if (block_no < 0) {
                        kfree(io_buf);
                        return bytes_written ? (int_32) bytes_written
                                             : block_no;
                }

                if (block_offset || chunk < ZONE_SIZE) {
                        memset(io_buf, 0, ZONE_SIZE);
                        if (file_block_idx * ZONE_SIZE < finode->i_size) {
                                if (read_blocks(inode->i_sb, block_no, 1,
                                                io_buf) < 0) {
                                        kfree(io_buf);
                                        return bytes_written
                                                   ? (int_32) bytes_written
                                                   : -EIO;
                                }
                        }
                        memcpy(io_buf + block_offset, write_cursor, chunk);
                        if (write_blocks(inode->i_sb, block_no, 1, io_buf) <
                            0) {
                                kfree(io_buf);
                                return bytes_written ? (int_32) bytes_written
                                                     : -EIO;
                        }
                } else {
                        if (write_blocks(inode->i_sb, block_no, 1,
                                         (uint_8 *) write_cursor) < 0) {
                                kfree(io_buf);
                                return bytes_written ? (int_32) bytes_written
                                                     : -EIO;
                        }
                }

                write_cursor += chunk;
                bytes_written += chunk;
        }

        file->f_pos += bytes_written;
        if (file->f_pos > finode->i_size)
                finode->i_size = file->f_pos;
        frogfs_fill_vfs_inode(inode, finode);
        inode->i_dirty = true;
        flush_inode(inode->i_sb, inode, io_buf);

        kfree(io_buf);
        return bytes_written;
}

int_32 frogfs_lseek(struct file *file, int_32 offset, uint_8 whence)
{
        if (!file)
                return -EINVAL;

        switch (whence) {
        case SEEK_SET:
                file->f_pos = offset;
                break;
        case SEEK_CUR:
                file->f_pos += offset;
                break;
        case SEEK_END:
                file->f_pos = file->f_inode->i_size + offset;
                break;
        default:
                return -EINVAL;
        }
        return file->f_pos;
}

int_32 frogfs_create(struct inode *dir, struct dentry *target, uint_32 mode)
{
        if (!dir || !target || !target->d_name)
                return -EINVAL;
        if (!dir->i_sb || !dir->i_private)
                return -EINVAL;

        struct frogfs_inode *dir_finode = dir->i_private;
        if (GET_FILE_TYPE(dir_finode->i_mode) != FT_DIRECTORY)
                return -ENOTDIR;

        uint_32 name_len = strlen(target->d_name);
        if (name_len == 0)
                return -EINVAL;
        if (name_len >= FROGFS_DIRENT_NAME_MAX)
                return -ENAMETOOLONG;

        int_32 existing = frogfs_lookup_dir_entry(dir, target->d_name);
        if (existing >= 0)
                return -EEXIST;
        if (existing != -ENOENT)
                return existing;

        struct inode *inode = frogfs_alloc_inode(dir->i_sb);
        if (!inode)
                return -ENOSPC;

        struct frogfs_inode *finode = inode->i_private;
        finode->i_mode = frogfs_regular_mode(mode);
        if (GET_FILE_TYPE(finode->i_mode) == FT_UNKOWN)
                finode->i_mode = FT_REGULAR << 11;
        finode->i_nlinks = 1;

        frogfs_fill_vfs_inode(inode, finode);
        inode->i_op = &frog_iop;
        inode->i_fop = &frog_fop;
        inode->i_sb = dir->i_sb;
        inode->i_dirty = true;

        struct frogfs_dir_entry entry;
        frogfs_init_dir_entry(&entry, target->d_name, inode->i_num,
                              FT_REGULAR);

        uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
        if (!io_buf) {
                frogfs_destory_inode(dir->i_sb, inode);
                return -ENOMEM;
        }

        int_32 ret = frogfs_add_dir_entry(dir, &entry, io_buf);
        if (ret < 0) {
                kfree(io_buf);
                frogfs_destory_inode(dir->i_sb, inode);
                return ret;
        }

        flush_inode(dir->i_sb, dir, io_buf);
        memset(io_buf, 0, 2 * ZONE_SIZE);
        flush_inode(dir->i_sb, inode, io_buf);
        flush_bitmap_block(dir->i_sb, INODE_BITMAP, inode->i_num);

        target->d_inode = inode;
        target->d_type = FT_REGULAR;
        target->d_sb = dir->i_sb;
        target->d_mounted = false;
        INIT_LIST_HEAD(&target->d_subdirs);
        list_add_tail(&inode->i_active_node, &dir->i_sb->s_inodes);

        kfree(io_buf);
        return 0;
}

int_32 frogfs_mkdir(struct inode *dir, struct dentry *target, uint_32 mode)
{
        if (!dir || !target || !target->d_name)
                return -EINVAL;
        if (!dir->i_sb || !dir->i_private)
                return -EINVAL;

        struct frogfs_inode *dir_finode = dir->i_private;
        if (GET_FILE_TYPE(dir_finode->i_mode) != FT_DIRECTORY)
                return -ENOTDIR;

        uint_32 name_len = strlen(target->d_name);
        if (name_len == 0)
                return -EINVAL;
        if (name_len >= FROGFS_DIRENT_NAME_MAX)
                return -ENAMETOOLONG;

        int_32 existing = frogfs_lookup_dir_entry(dir, target->d_name);
        if (existing >= 0)
                return -EEXIST;
        if (existing != -ENOENT)
                return existing;

        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        if (!frogfs_valid_dir_entry_size(entry_size))
                return -EIO;

        struct inode *inode = frogfs_alloc_inode(dir->i_sb);
        if (!inode)
                return -ENOSPC;

        uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
        if (!io_buf) {
                frogfs_destory_inode(dir->i_sb, inode);
                return -ENOMEM;
        }

        int_32 block_no = frogfs_alloc_zone_block(dir->i_sb);
        if (block_no < 0) {
                kfree(io_buf);
                frogfs_destory_inode(dir->i_sb, inode);
                return block_no;
        }

        struct frogfs_inode *finode = inode->i_private;
        finode->i_mode = frogfs_directory_mode(mode);
        finode->i_nlinks = 2;
        finode->i_zones[0] = block_no;
        finode->i_blocks = 1;
        finode->i_size = 2 * entry_size;

        frogfs_fill_vfs_inode(inode, finode);
        inode->i_op = &frog_iop;
        inode->i_fop = &frog_fop;
        inode->i_sb = dir->i_sb;
        inode->i_dirty = true;

        memset(io_buf, 0, ZONE_SIZE);
        struct frogfs_dir_entry *dot = (struct frogfs_dir_entry *) io_buf;
        struct frogfs_dir_entry *dotdot =
            (struct frogfs_dir_entry *) (io_buf + entry_size);
        frogfs_init_dir_entry(dot, ".", inode->i_num, FT_DIRECTORY);
        frogfs_init_dir_entry(dotdot, "..", dir->i_num, FT_DIRECTORY);

        if (write_blocks(dir->i_sb, block_no, 1, io_buf) < 0) {
                frogfs_free_zone_block(dir->i_sb, block_no);
                kfree(io_buf);
                frogfs_destory_inode(dir->i_sb, inode);
                return -EIO;
        }

        struct frogfs_dir_entry entry;
        frogfs_init_dir_entry(&entry, target->d_name, inode->i_num,
                              FT_DIRECTORY);

        int_32 ret = frogfs_add_dir_entry(dir, &entry, io_buf);
        if (ret < 0) {
                frogfs_free_zone_block(dir->i_sb, block_no);
                kfree(io_buf);
                frogfs_destory_inode(dir->i_sb, inode);
                return ret;
        }

        flush_inode(dir->i_sb, dir, io_buf);
        memset(io_buf, 0, 2 * ZONE_SIZE);
        flush_inode(dir->i_sb, inode, io_buf);
        flush_bitmap_block(dir->i_sb, INODE_BITMAP, inode->i_num);

        target->d_inode = inode;
        target->d_type = FT_DIRECTORY;
        target->d_sb = dir->i_sb;
        target->d_mounted = false;
        INIT_LIST_HEAD(&target->d_subdirs);
        list_add_tail(&inode->i_active_node, &dir->i_sb->s_inodes);

        kfree(io_buf);
        return 0;
}

int_32 frogfs_rmdir(struct inode *dir, struct dentry *target)
{
        if (!dir || !target || !target->d_name || !target->d_inode)
                return -EINVAL;
        if (!dir->i_sb || !dir->i_private)
                return -EINVAL;

        struct inode *inode = target->d_inode;
        struct frogfs_inode *finode = inode->i_private;
        if (!finode)
                return -EINVAL;
        if (GET_FILE_TYPE(finode->i_mode) != FT_DIRECTORY)
                return -ENOTDIR;

        struct super_block *sb = dir->i_sb;
        uint_32 entry_size = frogfs_dir_entry_size(sb);
        if (!frogfs_valid_dir_entry_size(entry_size))
                return -EIO;

        uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
        if (!io_buf)
                return -ENOMEM;

        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = DIV_ROUND_UP(finode->i_size, entry_size);
        for (uint_32 idx = 0; idx < entry_count; idx++) {
                uint_32 file_block_idx = idx / entries_per_zone;
                uint_32 block_entry_idx = idx % entries_per_zone;
                uint_32 block_offset = block_entry_idx * entry_size;
                int_32 block_no =
                    frogfs_get_file_block(inode, file_block_idx, false);
                if (block_no <= 0)
                        continue;
                if (read_blocks(sb, block_no, 1, io_buf) < 0) {
                        kfree(io_buf);
                        return -EIO;
                }
                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *) (io_buf + block_offset);
                if (frogfs_dir_entry_is_empty(entry))
                        continue;
                if (strcmp(entry->filename, ".") == 0 ||
                    strcmp(entry->filename, "..") == 0)
                        continue;
                kfree(io_buf);
                return -ENOTEMPTY;
        }

        for (uint_32 i = 0; i < FROGFS_DIRECT_ZONE_COUNT; i++) {
                if (finode->i_zones[i])
                        frogfs_free_zone_block(sb, finode->i_zones[i]);
        }

        int_32 ret = frogfs_remove_dir_entry(dir, target->d_name, io_buf);
        if (ret < 0) {
                kfree(io_buf);
                return ret;
        }

        flush_inode(sb, dir, io_buf);
        free_inode_bitmap(sb, inode->i_num);
        flush_bitmap_block(sb, INODE_BITMAP, inode->i_num);

        list_del(&inode->i_active_node);
        kfree(inode->i_private);
        kfree(inode);
        target->d_inode = NULL;

        kfree(io_buf);
        return 0;
}

int_32 frogfs_unlink(struct inode *dir, struct dentry *target)
{
        if (!dir || !target || !target->d_name || !target->d_inode)
                return -EINVAL;
        if (!dir->i_sb || !dir->i_private)
                return -EINVAL;

        struct inode *inode = target->d_inode;
        struct frogfs_inode *finode = inode->i_private;
        if (!finode)
                return -EINVAL;
        if (GET_FILE_TYPE(finode->i_mode) == FT_DIRECTORY)
                return -EISDIR;

        struct super_block *sb = dir->i_sb;
        uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
        if (!io_buf)
                return -ENOMEM;

        int_32 ret = frogfs_remove_dir_entry(dir, target->d_name, io_buf);
        if (ret < 0) {
                kfree(io_buf);
                return ret;
        }

        flush_inode(sb, dir, io_buf);

        for (uint_32 i = 0; i < FROGFS_DIRECT_ZONE_COUNT; i++) {
                if (finode->i_zones[i])
                        frogfs_free_zone_block(sb, finode->i_zones[i]);
        }

        for (uint_32 t = FROGFS_DIRECT_ZONE_COUNT; t < ZONE_IDX_MAX; t++) {
                if (!finode->i_zones[t])
                        continue;
                uint_32 *entries = (uint_32 *) io_buf;
                memset(io_buf, 0, ZONE_SIZE);
                if (read_blocks(sb, finode->i_zones[t], 1, io_buf) >= 0) {
                        uint_32 n = ZONE_SIZE / sizeof(uint_32);
                        for (uint_32 e = 0; e < n; e++) {
                                if (entries[e])
                                        frogfs_free_zone_block(sb, entries[e]);
                        }
                }
                frogfs_free_zone_block(sb, finode->i_zones[t]);
        }

        free_inode_bitmap(sb, inode->i_num);
        flush_bitmap_block(sb, INODE_BITMAP, inode->i_num);

        list_del(&inode->i_active_node);
        kfree(inode->i_private);
        kfree(inode);
        target->d_inode = NULL;

        kfree(io_buf);
        return 0;
}

struct dentry *frogfs_lookup(struct inode *dir, struct dentry *target)
{
        if (!dir || !target || !target->d_name || !dir->i_sb || !dir->i_private)
                return NULL;

        int_32 inode_no = frogfs_lookup_dir_entry(dir, target->d_name);
        if (inode_no < 0)
                return NULL;

        struct inode *inode = geti(dir->i_sb, (uint_32) inode_no);
        if (!inode)
                return NULL;

        inode->i_op  = &frog_iop;
        inode->i_fop = &frog_fop;
        inode->i_sb  = dir->i_sb;
        frogfs_fill_vfs_inode(inode, inode->i_private);

        struct frogfs_inode *finode = inode->i_private;
        target->d_inode   = inode;
        target->d_sb      = dir->i_sb;
        target->d_type    = (GET_FILE_TYPE(finode->i_mode) == FT_DIRECTORY)
                                ? FT_DIRECTORY : FT_REGULAR;
        target->d_mounted = false;
        INIT_LIST_HEAD(&target->d_subdirs);
        dentry_add_child(target->d_parent, target);

        return target;
}

int_32 frogfs_rename(struct inode *old_dir,
                     struct dentry *old_dentry,
                     struct inode *new_dir,
                     struct dentry *new_dentry)
{
        (void) old_dir;
        (void) old_dentry;
        (void) new_dir;
        (void) new_dentry;
        return -ENOSYS;
}

int_32 frogfs_link(struct dentry *old_dentry,
                   struct inode *dir,
                   struct dentry *new_dentry)
{
        (void) old_dentry;
        (void) dir;
        (void) new_dentry;
        return -ENOSYS;
}

int_32 frogfs_symlink(struct inode *dir,
                      struct dentry *target,
                      const char *symname)
{
        (void) dir;
        (void) target;
        (void) symname;
        return -ENOSYS;
}

int_32 frogfs_mmap(struct file *file, void *addr, uint_32 length, uint_32 flag)
{
        (void) file;
        (void) addr;
        (void) length;
        (void) flag;
        return -ENOSYS;
}


extern struct dentry *global_root_dentry;
static int make_mount_point(struct dentry *root, char *path)
{
        struct dentry *child = kmalloc(sizeof(struct dentry));
        if (!child)
                return -1;
        child->d_name = "test";
        child->d_parent = root;
        child->d_mounted = false;
        child->d_type = FT_DIRECTORY;
        child->d_inode = kmalloc(sizeof(struct inode));
        INIT_LIST_HEAD(&child->d_subdirs);
        list_add_tail(&child->d_child_node, &root->d_subdirs);

        if (vfs_mkdir(global_root_dentry, child) == -1) {
                return -1;
        }
        return 0;
}

int frogfs_init(void)
{
        register_fs(&frogfs_type);
        make_mount_point(global_root_dentry, "test");
        return 0;
}
