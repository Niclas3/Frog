#include "frogfs.h"
#include <frog/block.h>
#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/mount.h>
#include <kernel/vfs.h>
#include "ffs_utils.h"
#include "inode.h"
#include "super_block.h"

#define FROGFS_DIRECT_ZONE_COUNT 11
#define FROGFS_INDIRECT_TABLE_COUNT \
        (ZONE_IDX_MAX - FROGFS_DIRECT_ZONE_COUNT)
#define FROGFS_INDIRECT_ENTRY_COUNT (ZONE_SIZE / sizeof(uint_32))
#define FROGFS_DIRENT_NAME_MAX 16

struct frogfs_dir_entry {
        char filename[FROGFS_DIRENT_NAME_MAX];
        uint_32 i_no;
        enum file_type f_type;
};

STATIC_ASSERT(sizeof(struct frogfs_dir_entry) == FROGFS_DIR_ENTRY_SIZE,
              frogfs_dir_entry_size_must_match_disk_format);

struct frogfs_block_map {
        uint_32 *indirect[FROGFS_INDIRECT_TABLE_COUNT];
};

static struct super_block *frogfs_mount(struct fs_type *fs,
                                        int flags,
                                        const struct vfs_mount_source *source,
                                        void *data);

static struct fs_type frogfs_type = {
    .name = "frogfs",
    .mount = frogfs_mount,
};

static bool frogfs_registered;

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

static struct file_operations frog_fop = {
    .open = frogfs_open,
    .close = frogfs_close,
    .read = frogfs_read,
    .write = frogfs_write,
    .lseek = frogfs_lseek,
    .mmap = frogfs_mmap,
};

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

static void frogfs_fill_vfs_inode(struct inode *inode,
                                  struct frogfs_inode *disk_inode)
{
        inode->i_num = disk_inode->i_num;
        inode->i_mode = disk_inode->i_mode;
        inode->i_size = disk_inode->i_size;
        inode->i_nlink = disk_inode->i_nlinks;
        inode->i_uid = disk_inode->i_uid;
        inode->i_gid = disk_inode->i_gid;
        inode->i_atime = disk_inode->i_atime;
        inode->i_ctime = disk_inode->i_ctime;
        inode->i_mtime = disk_inode->i_mtime;
        inode->i_dev = disk_inode->i_dev;
}

static int frogfs_restore_inode_state(struct inode *inode,
                                      const struct frogfs_inode *saved,
                                      uint_8 *io_buf)
{
        struct frogfs_inode *disk_inode = inode->i_private;
        *disk_inode = *saved;
        frogfs_fill_vfs_inode(inode, disk_inode);
        int ret = flush_inode(inode->i_sb, inode, io_buf);
        if (ret < 0) {
                inode->i_dirty = true;
                frogfs_mark_needs_fsck(inode->i_sb);
        } else {
                inode->i_dirty = false;
        }
        return ret;
}

static bool frogfs_bitmap_bit_is_set(struct bitmap *bitmap, uint_32 bit)
{
        return bitmap && bitmap->bits &&
               bit < bitmap->map_bytes_length * 8 &&
               get_value_bitmap(bitmap, bit);
}

static bool frogfs_is_read_only(const struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return true;
        return ((const struct frogfs_super_block *) sb->s_fs_info)
                   ->disk_sb.s_rd_only != 0;
}

static struct frogfs_super_block *frogfs_lock_instance(struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return NULL;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        lock_fetch(&fsb->fs_lock);
        return fsb;
}

static void frogfs_unlock_instance(struct frogfs_super_block *fsb)
{
        lock_release(&fsb->fs_lock);
}

static uint_32 frogfs_max_file_size(struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return 0;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        return MIN(fsb->disk_sb.s_max_file_sz, (uint_32) MAX_FILE_SIZE);
}

static bool frogfs_valid_data_block(struct super_block *sb,
                                    uint_32 block_no,
                                    bool allow_root_zone)
{
        if (!sb || !sb->s_fs_info || block_no == 0)
                return false;

        struct frogfs_super_block *fsb = sb->s_fs_info;
        if (block_no < fsb->disk_sb.s_data_start_blk)
                return false;
        uint_32 zone_idx = block_no - fsb->disk_sb.s_data_start_blk;
        if (zone_idx >= fsb->disk_sb.s_nzones ||
            (!allow_root_zone && zone_idx == 0))
                return false;
        return frogfs_bitmap_bit_is_set(fsb->z_bmap, zone_idx);
}

static int frogfs_alloc_zone_block(struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return -EINVAL;
        if (frogfs_is_read_only(sb))
                return -EROFS;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        int_32 zone_idx = alloc_zone_bitmap(sb);
        if (zone_idx < 0)
                return zone_idx;

        int ret = flush_bitmap_block(sb, ZONE_BITMAP, zone_idx);
        if (ret < 0) {
                int rollback_ret = free_znode_bitmap(sb, zone_idx);
                if (rollback_ret < 0 ||
                    flush_bitmap_block(sb, ZONE_BITMAP, zone_idx) < 0)
                        frogfs_mark_needs_fsck(sb);
                return ret;
        }
        return (int_32) (fsb->disk_sb.s_data_start_blk + zone_idx);
}

static int frogfs_free_zone_block(struct super_block *sb, uint_32 block_no)
{
        if (!frogfs_valid_data_block(sb, block_no, false))
                return -EUCLEAN;

        struct frogfs_super_block *fsb = sb->s_fs_info;
        uint_32 zone_idx = block_no - fsb->disk_sb.s_data_start_blk;
        int ret = free_znode_bitmap(sb, (int_32) zone_idx);
        if (ret < 0)
                return ret;

        ret = flush_bitmap_block(sb, ZONE_BITMAP, (int_32) zone_idx);
        if (ret < 0) {
                unsigned long irq_flags;
                local_irq_save(irq_flags);
                set_value_bitmap(fsb->z_bmap, zone_idx, 1);
                local_irq_restore(irq_flags);
                if (flush_bitmap_block(sb, ZONE_BITMAP,
                                       (int_32) zone_idx) < 0)
                        frogfs_mark_needs_fsck(sb);
        }
        return ret;
}

static void frogfs_discard_allocated_zone(struct super_block *sb,
                                          uint_32 block_no)
{
        if (frogfs_free_zone_block(sb, block_no) < 0)
                frogfs_mark_needs_fsck(sb);
}

static int frogfs_zero_zone(struct super_block *sb, uint_32 block_no)
{
        uint_8 *buf = kmalloc(ZONE_SIZE);
        if (!buf)
                return -ENOMEM;
        memset(buf, 0, ZONE_SIZE);
        int ret = write_blocks(sb, block_no, 1, buf);
        kfree(buf);
        return ret;
}

static int frogfs_get_file_block(struct inode *inode,
                                 uint_32 file_block_idx,
                                 bool create,
                                 bool *allocated)
{
        if (allocated)
                *allocated = false;
        if (!inode || !inode->i_sb || !inode->i_private)
                return -EINVAL;
        if (file_block_idx >= MAX_ZONE_COUNT)
                return -EFBIG;

        struct frogfs_inode *disk_inode = inode->i_private;
        struct super_block *sb = inode->i_sb;
        bool required = file_block_idx <
                        DIV_ROUND_UP(disk_inode->i_size, ZONE_SIZE);

        if (file_block_idx < FROGFS_DIRECT_ZONE_COUNT) {
                uint_32 block_no = disk_inode->i_zones[file_block_idx];
                if (block_no &&
                    !frogfs_valid_data_block(sb, block_no,
                                             inode->i_num == 0))
                        return -EUCLEAN;
                if (!block_no && create) {
                        int ret = frogfs_alloc_zone_block(sb);
                        if (ret < 0)
                                return ret;
                        block_no = (uint_32) ret;
                        disk_inode->i_zones[file_block_idx] = block_no;
                        disk_inode->i_blocks++;
                        if (allocated)
                                *allocated = true;
                }
                if (!block_no && required)
                        return -EUCLEAN;
                return (int_32) block_no;
        }

        uint_32 relative = file_block_idx - FROGFS_DIRECT_ZONE_COUNT;
        uint_32 table_idx = relative / FROGFS_INDIRECT_ENTRY_COUNT;
        uint_32 entry_idx = relative % FROGFS_INDIRECT_ENTRY_COUNT;
        if (table_idx >= FROGFS_INDIRECT_TABLE_COUNT)
                return -EFBIG;

        uint_32 table_slot = FROGFS_DIRECT_ZONE_COUNT + table_idx;
        uint_32 table_block = disk_inode->i_zones[table_slot];
        bool new_table = false;
        if (table_block &&
            !frogfs_valid_data_block(sb, table_block, false))
                return -EUCLEAN;
        if (!table_block) {
                if (!create)
                        return required ? -EUCLEAN : 0;
                int ret = frogfs_alloc_zone_block(sb);
                if (ret < 0)
                        return ret;
                table_block = (uint_32) ret;
                ret = frogfs_zero_zone(sb, table_block);
                if (ret < 0) {
                        frogfs_discard_allocated_zone(sb, table_block);
                        return ret;
                }
                disk_inode->i_zones[table_slot] = table_block;
                disk_inode->i_blocks++;
                new_table = true;
        }

        uint_32 *entries = kmalloc(ZONE_SIZE);
        if (!entries) {
                if (new_table) {
                        disk_inode->i_zones[table_slot] = 0;
                        disk_inode->i_blocks--;
                        frogfs_discard_allocated_zone(sb, table_block);
                }
                return -ENOMEM;
        }

        int ret = read_blocks(sb, table_block, 1, (uint_8 *) entries);
        if (ret < 0) {
                kfree(entries);
                if (new_table) {
                        disk_inode->i_zones[table_slot] = 0;
                        disk_inode->i_blocks--;
                        frogfs_discard_allocated_zone(sb, table_block);
                }
                return ret;
        }

        uint_32 block_no = entries[entry_idx];
        if (block_no && !frogfs_valid_data_block(sb, block_no, false)) {
                kfree(entries);
                return -EUCLEAN;
        }
        if (!block_no && create) {
                ret = frogfs_alloc_zone_block(sb);
                if (ret < 0) {
                        kfree(entries);
                        if (new_table) {
                                disk_inode->i_zones[table_slot] = 0;
                                disk_inode->i_blocks--;
                                frogfs_discard_allocated_zone(sb,
                                                             table_block);
                        }
                        return ret;
                }
                block_no = (uint_32) ret;
                entries[entry_idx] = block_no;
                ret = write_blocks(sb, table_block, 1,
                                   (uint_8 *) entries);
                if (ret < 0) {
                        entries[entry_idx] = 0;
                        if (write_blocks(sb, table_block, 1,
                                         (uint_8 *) entries) < 0) {
                                frogfs_mark_needs_fsck(sb);
                                kfree(entries);
                                return ret;
                        }
                        frogfs_discard_allocated_zone(sb, block_no);
                        block_no = 0;
                        if (new_table) {
                                disk_inode->i_zones[table_slot] = 0;
                                disk_inode->i_blocks--;
                                frogfs_discard_allocated_zone(sb,
                                                             table_block);
                        }
                        kfree(entries);
                        return ret;
                }
                disk_inode->i_blocks++;
                if (allocated)
                        *allocated = true;
        }

        kfree(entries);
        if (!block_no && required)
                return -EUCLEAN;
        return (int_32) block_no;
}

static int frogfs_unmap_new_file_block(
    struct inode *inode,
    uint_32 file_block_idx,
    const struct frogfs_inode *old_inode)
{
        struct frogfs_inode *disk_inode = inode->i_private;
        struct super_block *sb = inode->i_sb;
        if (file_block_idx < FROGFS_DIRECT_ZONE_COUNT) {
                uint_32 block_no = disk_inode->i_zones[file_block_idx];
                if (!block_no)
                        return 0;
                int ret = frogfs_free_zone_block(sb, block_no);
                if (ret < 0)
                        ret = frogfs_free_zone_block(sb, block_no);
                if (ret < 0) {
                        frogfs_mark_needs_fsck(sb);
                        return ret;
                }
                disk_inode->i_zones[file_block_idx] = 0;
                if (disk_inode->i_blocks)
                        disk_inode->i_blocks--;
                return 0;
        }

        uint_32 relative = file_block_idx - FROGFS_DIRECT_ZONE_COUNT;
        uint_32 table_idx = relative / FROGFS_INDIRECT_ENTRY_COUNT;
        uint_32 entry_idx = relative % FROGFS_INDIRECT_ENTRY_COUNT;
        if (table_idx >= FROGFS_INDIRECT_TABLE_COUNT)
                return -EFBIG;

        uint_32 table_slot = FROGFS_DIRECT_ZONE_COUNT + table_idx;
        uint_32 table_block = disk_inode->i_zones[table_slot];
        if (!frogfs_valid_data_block(sb, table_block, false))
                return -EUCLEAN;

        uint_32 *entries = kmalloc(ZONE_SIZE);
        if (!entries)
                return -ENOMEM;
        int ret = read_blocks(sb, table_block, 1, (uint_8 *) entries);
        if (ret < 0) {
                kfree(entries);
                return ret;
        }

        uint_32 block_no = entries[entry_idx];
        entries[entry_idx] = 0;
        ret = write_blocks(sb, table_block, 1, (uint_8 *) entries);
        if (ret < 0) {
                entries[entry_idx] = block_no;
                if (write_blocks(sb, table_block, 1,
                                 (uint_8 *) entries) < 0)
                        frogfs_mark_needs_fsck(sb);
                kfree(entries);
                return ret;
        }
        if (block_no) {
                ret = frogfs_free_zone_block(sb, block_no);
                if (ret < 0)
                        ret = frogfs_free_zone_block(sb, block_no);
                if (ret < 0) {
                        entries[entry_idx] = block_no;
                        if (write_blocks(sb, table_block, 1,
                                         (uint_8 *) entries) < 0)
                                frogfs_mark_needs_fsck(sb);
                        kfree(entries);
                        return ret;
                }
                if (disk_inode->i_blocks)
                        disk_inode->i_blocks--;
        }

        bool empty = true;
        for (uint_32 idx = 0; idx < FROGFS_INDIRECT_ENTRY_COUNT; idx++) {
                if (entries[idx]) {
                        empty = false;
                        break;
                }
        }
        kfree(entries);
        bool table_was_new =
            !old_inode || old_inode->i_zones[table_slot] == 0;
        if (empty && table_was_new) {
                int table_ret = frogfs_free_zone_block(sb, table_block);
                if (table_ret < 0)
                        table_ret = frogfs_free_zone_block(sb, table_block);
                if (table_ret == 0) {
                        disk_inode->i_zones[table_slot] = 0;
                        if (disk_inode->i_blocks)
                                disk_inode->i_blocks--;
                } else {
                        frogfs_mark_needs_fsck(sb);
                }
                if (ret == 0)
                        ret = table_ret;
        }
        return ret;
}

static void frogfs_free_block_map_buffers(struct frogfs_block_map *map)
{
        for (uint_32 idx = 0; idx < FROGFS_INDIRECT_TABLE_COUNT; idx++) {
                if (map->indirect[idx]) {
                        kfree(map->indirect[idx]);
                        map->indirect[idx] = NULL;
                }
        }
}

static uint_32 frogfs_block_occurrences(struct frogfs_inode *disk_inode,
                                        struct frogfs_block_map *map,
                                        uint_32 block_no)
{
        uint_32 count = 0;
        for (uint_32 idx = 0; idx < FROGFS_DIRECT_ZONE_COUNT; idx++) {
                if (disk_inode->i_zones[idx] == block_no)
                        count++;
        }
        for (uint_32 table = 0; table < FROGFS_INDIRECT_TABLE_COUNT;
             table++) {
                if (disk_inode->i_zones[FROGFS_DIRECT_ZONE_COUNT + table] ==
                    block_no)
                        count++;
                if (!map->indirect[table])
                        continue;
                for (uint_32 entry = 0; entry < FROGFS_INDIRECT_ENTRY_COUNT;
                     entry++) {
                        if (map->indirect[table][entry] == block_no)
                                count++;
                }
        }
        return count;
}

static uint_32 frogfs_block_map_get(struct frogfs_inode *disk_inode,
                                    struct frogfs_block_map *map,
                                    uint_32 file_block_idx)
{
        if (file_block_idx < FROGFS_DIRECT_ZONE_COUNT)
                return disk_inode->i_zones[file_block_idx];
        uint_32 relative = file_block_idx - FROGFS_DIRECT_ZONE_COUNT;
        uint_32 table = relative / FROGFS_INDIRECT_ENTRY_COUNT;
        uint_32 entry = relative % FROGFS_INDIRECT_ENTRY_COUNT;
        if (table >= FROGFS_INDIRECT_TABLE_COUNT || !map->indirect[table])
                return 0;
        return map->indirect[table][entry];
}

static int frogfs_load_block_map(struct inode *inode,
                                 struct frogfs_block_map *map)
{
        if (!inode || !inode->i_private || !inode->i_sb || !map)
                return -EINVAL;
        memset(map, 0, sizeof(*map));
        struct frogfs_inode *disk_inode = inode->i_private;
        uint_32 physical_blocks = 0;

        for (uint_32 idx = 0; idx < FROGFS_DIRECT_ZONE_COUNT; idx++) {
                uint_32 block_no = disk_inode->i_zones[idx];
                if (!block_no)
                        continue;
                if (!frogfs_valid_data_block(inode->i_sb, block_no,
                                             inode->i_num == 0))
                        goto corrupt;
                physical_blocks++;
        }

        for (uint_32 table = 0; table < FROGFS_INDIRECT_TABLE_COUNT;
             table++) {
                uint_32 table_block =
                    disk_inode->i_zones[FROGFS_DIRECT_ZONE_COUNT + table];
                if (!table_block)
                        continue;
                if (!frogfs_valid_data_block(inode->i_sb, table_block, false))
                        goto corrupt;

                map->indirect[table] = kmalloc(ZONE_SIZE);
                if (!map->indirect[table]) {
                        frogfs_free_block_map_buffers(map);
                        return -ENOMEM;
                }
                if (read_blocks(inode->i_sb, table_block, 1,
                                (uint_8 *) map->indirect[table]) < 0)
                        goto corrupt;
                physical_blocks++;

                for (uint_32 entry = 0;
                     entry < FROGFS_INDIRECT_ENTRY_COUNT; entry++) {
                        uint_32 block_no = map->indirect[table][entry];
                        if (!block_no)
                                continue;
                        if (!frogfs_valid_data_block(inode->i_sb, block_no,
                                                     false))
                                goto corrupt;
                        physical_blocks++;
                }
        }

        for (uint_32 idx = 0; idx < FROGFS_DIRECT_ZONE_COUNT; idx++) {
                uint_32 block_no = disk_inode->i_zones[idx];
                if (block_no &&
                    frogfs_block_occurrences(disk_inode, map, block_no) != 1)
                        goto corrupt;
        }
        for (uint_32 table = 0; table < FROGFS_INDIRECT_TABLE_COUNT;
             table++) {
                uint_32 table_block =
                    disk_inode->i_zones[FROGFS_DIRECT_ZONE_COUNT + table];
                if (table_block &&
                    frogfs_block_occurrences(disk_inode, map, table_block) !=
                        1)
                        goto corrupt;
                if (!map->indirect[table])
                        continue;
                for (uint_32 entry = 0;
                     entry < FROGFS_INDIRECT_ENTRY_COUNT; entry++) {
                        uint_32 block_no = map->indirect[table][entry];
                        if (block_no &&
                            frogfs_block_occurrences(disk_inode, map,
                                                     block_no) != 1)
                                goto corrupt;
                }
        }

        uint_32 required = DIV_ROUND_UP(disk_inode->i_size, ZONE_SIZE);
        for (uint_32 idx = 0; idx < required; idx++) {
                if (!frogfs_block_map_get(disk_inode, map, idx))
                        goto corrupt;
        }
        if (physical_blocks != disk_inode->i_blocks)
                goto corrupt;
        return 0;

corrupt:
        frogfs_free_block_map_buffers(map);
        return -EUCLEAN;
}

static void frogfs_set_block_map_bits(struct super_block *sb,
                                      struct frogfs_inode *disk_inode,
                                      struct frogfs_block_map *map,
                                      uint_8 value)
{
        struct frogfs_super_block *fsb = sb->s_fs_info;
        unsigned long irq_flags;
        local_irq_save(irq_flags);
        for (uint_32 idx = 0; idx < FROGFS_DIRECT_ZONE_COUNT; idx++) {
                uint_32 block_no = disk_inode->i_zones[idx];
                if (block_no)
                        set_value_bitmap(
                            fsb->z_bmap,
                            block_no - fsb->disk_sb.s_data_start_blk, value);
        }
        for (uint_32 table = 0; table < FROGFS_INDIRECT_TABLE_COUNT;
             table++) {
                if (map->indirect[table]) {
                        for (uint_32 entry = 0;
                             entry < FROGFS_INDIRECT_ENTRY_COUNT; entry++) {
                                uint_32 block_no =
                                    map->indirect[table][entry];
                                if (block_no)
                                        set_value_bitmap(
                                            fsb->z_bmap,
                                            block_no -
                                                fsb->disk_sb.s_data_start_blk,
                                            value);
                        }
                }
                uint_32 table_block =
                    disk_inode->i_zones[FROGFS_DIRECT_ZONE_COUNT + table];
                if (table_block)
                        set_value_bitmap(
                            fsb->z_bmap,
                            table_block - fsb->disk_sb.s_data_start_blk,
                            value);
        }
        local_irq_restore(irq_flags);
}

static int frogfs_release_block_map(struct super_block *sb,
                                    struct frogfs_inode *disk_inode,
                                    struct frogfs_block_map *map)
{
        struct frogfs_super_block *fsb = sb->s_fs_info;
        frogfs_set_block_map_bits(sb, disk_inode, map, 0);
        int ret = write_bitmap(sb, fsb->disk_sb.s_zmap_blk,
                               fsb->disk_sb.s_zmap_sz, fsb->z_bmap);
        if (ret < 0) {
                frogfs_set_block_map_bits(sb, disk_inode, map, 1);
                if (write_bitmap(sb, fsb->disk_sb.s_zmap_blk,
                                 fsb->disk_sb.s_zmap_sz, fsb->z_bmap) < 0)
                        frogfs_mark_needs_fsck(sb);
        }
        return ret;
}

static int frogfs_truncate_inode(struct inode *inode)
{
        struct frogfs_block_map map;
        int ret = frogfs_load_block_map(inode, &map);
        if (ret < 0)
                return ret;

        struct frogfs_inode *disk_inode = inode->i_private;
        struct frogfs_inode old_inode = *disk_inode;
        uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
        if (!io_buf) {
                frogfs_free_block_map_buffers(&map);
                return -ENOMEM;
        }

        memset(disk_inode->i_zones, 0, sizeof(disk_inode->i_zones));
        disk_inode->i_blocks = 0;
        disk_inode->i_size = 0;
        frogfs_fill_vfs_inode(inode, disk_inode);
        ret = flush_inode(inode->i_sb, inode, io_buf);
        if (ret < 0) {
                frogfs_restore_inode_state(inode, &old_inode, io_buf);
        } else {
                inode->i_dirty = false;
                ret = frogfs_release_block_map(inode->i_sb, &old_inode, &map);
                if (ret < 0)
                        frogfs_restore_inode_state(inode, &old_inode, io_buf);
        }

        kfree(io_buf);
        frogfs_free_block_map_buffers(&map);
        return ret;
}

static uint_32 frogfs_dir_entry_size(struct super_block *sb)
{
        struct frogfs_super_block *fsb = sb->s_fs_info;
        return fsb->disk_sb.dir_entry_size;
}

static bool frogfs_dir_entry_is_empty(struct frogfs_dir_entry *entry)
{
        if (entry->filename[0] != '\0')
                return false;
        for (uint_32 idx = 1; idx < FROGFS_DIRENT_NAME_MAX; idx++) {
                if (entry->filename[idx] != '\0')
                        return false;
        }
        return entry->i_no == 0 && entry->f_type == FT_UNKOWN;
}

static int frogfs_bounded_name_length(const char *name)
{
        for (uint_32 len = 0; len < FROGFS_DIRENT_NAME_MAX; len++) {
                if (name[len] == '\0')
                        return (int) len;
        }
        return -ENAMETOOLONG;
}

static bool frogfs_dir_entry_name_eq(struct frogfs_dir_entry *entry,
                                     const char *name)
{
        int disk_len = frogfs_bounded_name_length(entry->filename);
        int name_len = frogfs_bounded_name_length(name);
        return disk_len >= 0 && disk_len == name_len &&
               strncmp(entry->filename, name, (uint_32) disk_len) == 0;
}

static bool frogfs_dir_entry_is_valid(struct super_block *sb,
                                      struct frogfs_dir_entry *entry)
{
        if (frogfs_dir_entry_is_empty(entry))
                return true;
        int name_len = frogfs_bounded_name_length(entry->filename);
        if (name_len <= 0 ||
            (entry->f_type != FT_REGULAR &&
             entry->f_type != FT_DIRECTORY))
                return false;
        for (int idx = 0; idx < name_len; idx++) {
                if (entry->filename[idx] == '/')
                        return false;
        }
        for (int idx = name_len + 1; idx < FROGFS_DIRENT_NAME_MAX; idx++) {
                if (entry->filename[idx] != '\0')
                        return false;
        }

        struct frogfs_super_block *fsb = sb->s_fs_info;
        return entry->i_no < fsb->disk_sb.s_ninodes &&
               frogfs_bitmap_bit_is_set(fsb->i_bmap, entry->i_no);
}

static void frogfs_init_dir_entry(struct frogfs_dir_entry *entry,
                                  const char *name,
                                  uint_32 inode_no,
                                  enum file_type type)
{
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->filename, name, FROGFS_DIRENT_NAME_MAX - 1);
        entry->i_no = inode_no;
        entry->f_type = type;
}

static int frogfs_lookup_dir_entry(struct inode *dir,
                                   const char *name,
                                   enum file_type *type_out)
{
        struct frogfs_inode *disk_inode = dir->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = disk_inode->i_size / entry_size;
        uint_8 *buf = kmalloc(ZONE_SIZE);
        if (!buf)
                return -ENOMEM;

        uint_32 loaded_block = 0xffffffffU;
        for (uint_32 entry_idx = 0; entry_idx < entry_count; entry_idx++) {
                uint_32 file_block_idx = entry_idx / entries_per_zone;
                uint_32 block_entry_idx = entry_idx % entries_per_zone;
                if (file_block_idx != loaded_block) {
                        int block_no = frogfs_get_file_block(
                            dir, file_block_idx, false, NULL);
                        if (block_no < 0) {
                                kfree(buf);
                                return block_no;
                        }
                        if (read_blocks(dir->i_sb, (uint_32) block_no, 1,
                                        buf) < 0) {
                                kfree(buf);
                                return -EIO;
                        }
                        loaded_block = file_block_idx;
                }

                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *)
                        (buf + block_entry_idx * entry_size);
                if (!frogfs_dir_entry_is_valid(dir->i_sb, entry)) {
                        kfree(buf);
                        return -EUCLEAN;
                }
                if (!frogfs_dir_entry_is_empty(entry) &&
                    frogfs_dir_entry_name_eq(entry, name)) {
                        int inode_no = (int) entry->i_no;
                        if (type_out)
                                *type_out = entry->f_type;
                        kfree(buf);
                        return inode_no;
                }
        }

        kfree(buf);
        return -ENOENT;
}

static int frogfs_add_dir_entry(struct inode *dir,
                                struct frogfs_dir_entry *new_entry,
                                uint_8 *io_buf)
{
        struct frogfs_inode *disk_inode = dir->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = disk_inode->i_size / entry_size;

        for (uint_32 entry_idx = 0; entry_idx <= entry_count; entry_idx++) {
                bool appending = entry_idx == entry_count;
                if (appending) {
                        uint_32 max_file_size =
                            frogfs_max_file_size(dir->i_sb);
                        if (max_file_size < entry_size ||
                            disk_inode->i_size > max_file_size - entry_size)
                                return -EFBIG;
                }
                uint_32 file_block_idx = entry_idx / entries_per_zone;
                uint_32 block_entry_idx = entry_idx % entries_per_zone;
                struct frogfs_inode old_inode = *disk_inode;
                bool allocated = false;
                int block_no = frogfs_get_file_block(
                    dir, file_block_idx, appending, &allocated);
                if (block_no < 0)
                        return block_no;

                if (allocated) {
                        memset(io_buf, 0, ZONE_SIZE);
                        int ret = write_blocks(dir->i_sb,
                                               (uint_32) block_no, 1,
                                               io_buf);
                        if (ret < 0) {
                                if (frogfs_unmap_new_file_block(
                                        dir, file_block_idx, &old_inode) < 0)
                                        frogfs_mark_needs_fsck(dir->i_sb);
                                *disk_inode = old_inode;
                                frogfs_fill_vfs_inode(dir, disk_inode);
                                return ret;
                        }
                } else if (read_blocks(dir->i_sb, (uint_32) block_no, 1,
                                       io_buf) < 0) {
                        return -EIO;
                }
                memcpy(io_buf + 2 * ZONE_SIZE, io_buf, ZONE_SIZE);

                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *)
                        (io_buf + block_entry_idx * entry_size);
                if (!frogfs_dir_entry_is_valid(dir->i_sb, entry))
                        return -EUCLEAN;
                if (!appending && !frogfs_dir_entry_is_empty(entry))
                        continue;

                if (appending)
                        disk_inode->i_size += entry_size;
                frogfs_fill_vfs_inode(dir, disk_inode);

                if (allocated || appending) {
                        int ret = flush_inode(dir->i_sb, dir, io_buf);
                        if (ret < 0) {
                                if (allocated)
                                        if (frogfs_unmap_new_file_block(
                                                dir, file_block_idx,
                                                &old_inode) < 0)
                                                frogfs_mark_needs_fsck(
                                                    dir->i_sb);
                                frogfs_restore_inode_state(
                                    dir, &old_inode, io_buf);
                                return ret;
                        }
                        dir->i_dirty = false;
                        memcpy(io_buf, io_buf + 2 * ZONE_SIZE, ZONE_SIZE);
                        entry = (struct frogfs_dir_entry *)
                            (io_buf + block_entry_idx * entry_size);
                }

                memset(entry, 0, entry_size);
                memcpy(entry, new_entry, sizeof(*new_entry));
                int ret = write_blocks(dir->i_sb, (uint_32) block_no, 1,
                                       io_buf);
                if (ret == 0)
                        return 0;

                if (write_blocks(dir->i_sb, (uint_32) block_no, 1,
                                 io_buf + 2 * ZONE_SIZE) < 0)
                        frogfs_mark_needs_fsck(dir->i_sb);
                if (allocated &&
                    frogfs_unmap_new_file_block(dir, file_block_idx,
                                                &old_inode) < 0)
                        frogfs_mark_needs_fsck(dir->i_sb);
                if (allocated || appending)
                        frogfs_restore_inode_state(dir, &old_inode, io_buf);
                return ret;
        }
        return -ENOSPC;
}

static int frogfs_remove_dir_entry(struct inode *dir,
                                   const char *name,
                                   uint_8 *io_buf)
{
        struct frogfs_inode *disk_inode = dir->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = disk_inode->i_size / entry_size;

        uint_32 loaded_file_block = 0xffffffffU;
        int loaded_disk_block = 0;
        for (uint_32 entry_idx = 0; entry_idx < entry_count; entry_idx++) {
                uint_32 file_block_idx = entry_idx / entries_per_zone;
                uint_32 block_entry_idx = entry_idx % entries_per_zone;
                if (file_block_idx != loaded_file_block) {
                        loaded_disk_block = frogfs_get_file_block(
                            dir, file_block_idx, false, NULL);
                        if (loaded_disk_block < 0)
                                return loaded_disk_block;
                        if (read_blocks(dir->i_sb,
                                        (uint_32) loaded_disk_block, 1,
                                        io_buf) < 0)
                                return -EIO;
                        loaded_file_block = file_block_idx;
                }

                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *)
                        (io_buf + block_entry_idx * entry_size);
                if (!frogfs_dir_entry_is_valid(dir->i_sb, entry))
                        return -EUCLEAN;
                if (frogfs_dir_entry_is_empty(entry) ||
                    !frogfs_dir_entry_name_eq(entry, name))
                        continue;

                memcpy(io_buf + ZONE_SIZE, io_buf, ZONE_SIZE);
                memset(entry, 0, entry_size);
                int ret = write_blocks(dir->i_sb,
                                       (uint_32) loaded_disk_block, 1,
                                       io_buf);
                if (ret < 0 &&
                    write_blocks(dir->i_sb, (uint_32) loaded_disk_block, 1,
                                 io_buf + ZONE_SIZE) < 0)
                        frogfs_mark_needs_fsck(dir->i_sb);
                return ret;
        }
        return -ENOENT;
}

static int frogfs_rollback_added_dir_entry(
    struct inode *dir,
    const char *name,
    const struct frogfs_inode *old_inode,
    uint_8 *io_buf)
{
        int ret = frogfs_remove_dir_entry(dir, name, io_buf);
        if (ret < 0)
                frogfs_mark_needs_fsck(dir->i_sb);

        struct frogfs_inode *disk_inode = dir->i_private;
        if (disk_inode->i_blocks > old_inode->i_blocks) {
                uint_32 entries_per_zone =
                    ZONE_SIZE / frogfs_dir_entry_size(dir->i_sb);
                uint_32 file_block_idx =
                    (old_inode->i_size / frogfs_dir_entry_size(dir->i_sb)) /
                    entries_per_zone;
                if (frogfs_unmap_new_file_block(dir, file_block_idx,
                                                old_inode) < 0) {
                        frogfs_mark_needs_fsck(dir->i_sb);
                        if (ret == 0)
                                ret = -EIO;
                }
        }
        if (frogfs_restore_inode_state(dir, old_inode, io_buf) < 0 &&
            ret == 0)
                ret = -EIO;
        return ret;
}

struct frogfs_mount_scan {
        uint_8 *zone_seen;
        uint_8 *inode_type;
        uint_16 *namespace_refs;
        uint_16 *parent_refs;
        uint_16 *child_dirs;
        uint_32 *parent_of;
        uint_32 *dotdot;
        struct inode **inodes;
};

static void frogfs_free_mount_scan(struct frogfs_mount_scan *scan)
{
        if (scan->inodes)
                kfree(scan->inodes);
        if (scan->dotdot)
                kfree(scan->dotdot);
        if (scan->parent_of)
                kfree(scan->parent_of);
        if (scan->child_dirs)
                kfree(scan->child_dirs);
        if (scan->parent_refs)
                kfree(scan->parent_refs);
        if (scan->namespace_refs)
                kfree(scan->namespace_refs);
        if (scan->inode_type)
                kfree(scan->inode_type);
        if (scan->zone_seen)
                kfree(scan->zone_seen);
        memset(scan, 0, sizeof(*scan));
}

static int frogfs_claim_scan_zone(struct super_block *sb,
                                  struct frogfs_mount_scan *scan,
                                  uint_32 block_no)
{
        struct frogfs_super_block *fsb = sb->s_fs_info;
        if (block_no < fsb->disk_sb.s_data_start_blk)
                return -EUCLEAN;
        uint_32 zone = block_no - fsb->disk_sb.s_data_start_blk;
        if (zone >= fsb->disk_sb.s_nzones ||
            !frogfs_bitmap_bit_is_set(fsb->z_bmap, zone))
                return -EUCLEAN;
        uint_32 byte = zone / 8;
        uint_8 mask = (uint_8) (1U << (zone % 8));
        if (scan->zone_seen[byte] & mask)
                return -EUCLEAN;
        scan->zone_seen[byte] |= mask;
        return 0;
}

static int frogfs_claim_inode_zones(struct inode *inode,
                                    struct frogfs_block_map *map,
                                    struct frogfs_mount_scan *scan)
{
        struct frogfs_inode *disk_inode = inode->i_private;
        for (uint_32 idx = 0; idx < FROGFS_DIRECT_ZONE_COUNT; idx++) {
                uint_32 block_no = disk_inode->i_zones[idx];
                if (block_no &&
                    frogfs_claim_scan_zone(inode->i_sb, scan, block_no) < 0)
                        return -EUCLEAN;
        }
        for (uint_32 table = 0; table < FROGFS_INDIRECT_TABLE_COUNT;
             table++) {
                uint_32 table_block =
                    disk_inode->i_zones[FROGFS_DIRECT_ZONE_COUNT + table];
                if (table_block &&
                    frogfs_claim_scan_zone(inode->i_sb, scan,
                                           table_block) < 0)
                        return -EUCLEAN;
                if (!map->indirect[table])
                        continue;
                for (uint_32 entry = 0;
                     entry < FROGFS_INDIRECT_ENTRY_COUNT; entry++) {
                        uint_32 block_no = map->indirect[table][entry];
                        if (block_no &&
                            frogfs_claim_scan_zone(inode->i_sb, scan,
                                                   block_no) < 0)
                                return -EUCLEAN;
                }
        }
        return 0;
}

static uint_32 frogfs_dir_name_hash(const char *name)
{
        uint_32 hash = 2166136261U;
        int len = frogfs_bounded_name_length(name);
        for (int idx = 0; idx < len; idx++) {
                hash ^= (uint_8) name[idx];
                hash *= 16777619U;
        }
        return hash;
}

static int frogfs_scan_directory(struct inode *dir,
                                 struct frogfs_block_map *map,
                                 struct frogfs_mount_scan *scan)
{
        struct frogfs_super_block *fsb = dir->i_sb->s_fs_info;
        struct frogfs_inode *disk_inode = dir->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(dir->i_sb);
        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = disk_inode->i_size / entry_size;
        uint_8 *buf = kmalloc(ZONE_SIZE);
        uint_32 hash_capacity = 1;
        while (hash_capacity < entry_count * 2U)
                hash_capacity <<= 1;
        uint_32 *name_slots =
            kmalloc(hash_capacity * sizeof(*name_slots));
        char *names =
            kmalloc(entry_count * FROGFS_DIRENT_NAME_MAX);
        if (!buf || !name_slots || !names) {
                if (buf)
                        kfree(buf);
                if (name_slots)
                        kfree(name_slots);
                if (names)
                        kfree(names);
                return -ENOMEM;
        }
        memset(name_slots, 0, hash_capacity * sizeof(*name_slots));
        uint_32 name_count = 0;

        uint_32 dot_count = 0;
        uint_32 dotdot_count = 0;
        uint_32 loaded_block = 0xffffffffU;
        int ret = 0;
        for (uint_32 idx = 0; idx < entry_count; idx++) {
                uint_32 file_block = idx / entries_per_zone;
                if (file_block != loaded_block) {
                        uint_32 block_no =
                            frogfs_block_map_get(disk_inode, map, file_block);
                        if (!block_no ||
                            read_blocks(dir->i_sb, block_no, 1, buf) < 0) {
                                ret = -EIO;
                                goto out;
                        }
                        loaded_block = file_block;
                }

                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *)
                        (buf + (idx % entries_per_zone) * entry_size);
                if (!frogfs_dir_entry_is_valid(dir->i_sb, entry)) {
                        ret = -EUCLEAN;
                        goto out;
                }
                if (frogfs_dir_entry_is_empty(entry))
                        continue;

                uint_32 name_slot =
                    frogfs_dir_name_hash(entry->filename) &
                    (hash_capacity - 1);
                while (name_slots[name_slot]) {
                        const char *stored =
                            names + (name_slots[name_slot] - 1) *
                                        FROGFS_DIRENT_NAME_MAX;
                        if (memcmp(stored, entry->filename,
                                   FROGFS_DIRENT_NAME_MAX) == 0) {
                                ret = -EUCLEAN;
                                goto out;
                        }
                        name_slot = (name_slot + 1) & (hash_capacity - 1);
                }
                memcpy(names + name_count * FROGFS_DIRENT_NAME_MAX,
                       entry->filename, FROGFS_DIRENT_NAME_MAX);
                name_slots[name_slot] = ++name_count;

                uint_32 target = entry->i_no;
                if (target >= fsb->disk_sb.s_ninodes ||
                    scan->inode_type[target] != entry->f_type) {
                        ret = -EUCLEAN;
                        goto out;
                }
                if (frogfs_dir_entry_name_eq(entry, ".")) {
                        if (entry->f_type != FT_DIRECTORY ||
                            target != dir->i_num || ++dot_count != 1) {
                                ret = -EUCLEAN;
                                goto out;
                        }
                        continue;
                }
                if (frogfs_dir_entry_name_eq(entry, "..")) {
                        if (entry->f_type != FT_DIRECTORY ||
                            ++dotdot_count != 1) {
                                ret = -EUCLEAN;
                                goto out;
                        }
                        scan->dotdot[dir->i_num] = target;
                        continue;
                }

                if (scan->namespace_refs[target] == 0xffffU) {
                        ret = -EUCLEAN;
                        goto out;
                }
                scan->namespace_refs[target]++;
                if (entry->f_type != FT_DIRECTORY)
                        continue;
                if (target == fsb->disk_sb.root_inode_no ||
                    scan->parent_refs[target] == 0xffffU ||
                    scan->child_dirs[dir->i_num] == 0xffffU) {
                        ret = -EUCLEAN;
                        goto out;
                }
                scan->parent_refs[target]++;
                scan->parent_of[target] = dir->i_num;
                scan->child_dirs[dir->i_num]++;
        }
        if (dot_count != 1 || dotdot_count != 1)
                ret = -EUCLEAN;
out:
        kfree(names);
        kfree(name_slots);
        kfree(buf);
        return ret;
}

static int frogfs_scan_allocated_inodes(struct super_block *sb)
{
        struct frogfs_super_block *fsb = sb->s_fs_info;
        uint_32 ninodes = fsb->disk_sb.s_ninodes;
        uint_32 nzones = fsb->disk_sb.s_nzones;
        uint_32 zone_bytes = nzones / 8 + (nzones % 8 != 0);
        struct frogfs_mount_scan scan;
        memset(&scan, 0, sizeof(scan));
        scan.zone_seen = kmalloc(zone_bytes);
        scan.inode_type = kmalloc(ninodes * sizeof(*scan.inode_type));
        scan.namespace_refs =
            kmalloc(ninodes * sizeof(*scan.namespace_refs));
        scan.parent_refs = kmalloc(ninodes * sizeof(*scan.parent_refs));
        scan.child_dirs = kmalloc(ninodes * sizeof(*scan.child_dirs));
        scan.parent_of = kmalloc(ninodes * sizeof(*scan.parent_of));
        scan.dotdot = kmalloc(ninodes * sizeof(*scan.dotdot));
        scan.inodes = kmalloc(ninodes * sizeof(*scan.inodes));
        if (!scan.zone_seen || !scan.inode_type || !scan.namespace_refs ||
            !scan.parent_refs || !scan.child_dirs || !scan.parent_of ||
            !scan.dotdot || !scan.inodes) {
                frogfs_free_mount_scan(&scan);
                return -ENOMEM;
        }
        memset(scan.zone_seen, 0, zone_bytes);
        memset(scan.inode_type, 0, ninodes * sizeof(*scan.inode_type));
        memset(scan.namespace_refs, 0,
               ninodes * sizeof(*scan.namespace_refs));
        memset(scan.parent_refs, 0, ninodes * sizeof(*scan.parent_refs));
        memset(scan.child_dirs, 0, ninodes * sizeof(*scan.child_dirs));
        memset(scan.parent_of, 0xff, ninodes * sizeof(*scan.parent_of));
        memset(scan.dotdot, 0xff, ninodes * sizeof(*scan.dotdot));
        memset(scan.inodes, 0, ninodes * sizeof(*scan.inodes));

        int ret = 0;
        for (uint_32 ino = 0; ino < ninodes; ino++) {
                if (!frogfs_bitmap_bit_is_set(fsb->i_bmap, ino))
                        continue;
                struct inode *inode = geti(sb, ino);
                if (!inode) {
                        ret = -EUCLEAN;
                        goto out;
                }
                scan.inodes[ino] = inode;
                struct frogfs_inode *disk_inode = inode->i_private;
                scan.inode_type[ino] =
                    (uint_8) GET_FILE_TYPE(disk_inode->i_mode);
                struct frogfs_block_map map;
                ret = frogfs_load_block_map(inode, &map);
                if (ret < 0)
                        goto out;
                ret = frogfs_claim_inode_zones(inode, &map, &scan);
                frogfs_free_block_map_buffers(&map);
                if (ret < 0)
                        goto out;
        }

        for (uint_32 zone = 0; zone < nzones; zone++) {
                bool seen = scan.zone_seen[zone / 8] &
                            (uint_8) (1U << (zone % 8));
                if (seen != frogfs_bitmap_bit_is_set(fsb->z_bmap, zone)) {
                        ret = -EUCLEAN;
                        goto out;
                }
        }

        for (uint_32 ino = 0; ino < ninodes; ino++) {
                struct inode *inode = scan.inodes[ino];
                if (!inode || scan.inode_type[ino] != FT_DIRECTORY)
                        continue;
                struct frogfs_block_map map;
                ret = frogfs_load_block_map(inode, &map);
                if (ret < 0)
                        goto out;
                ret = frogfs_scan_directory(inode, &map, &scan);
                frogfs_free_block_map_buffers(&map);
                if (ret < 0)
                        goto out;
        }

        for (uint_32 ino = 0; ino < ninodes; ino++) {
                struct inode *inode = scan.inodes[ino];
                if (!inode)
                        continue;
                struct frogfs_inode *disk_inode = inode->i_private;
                if (scan.inode_type[ino] == FT_REGULAR) {
                        if (!scan.namespace_refs[ino] ||
                            scan.namespace_refs[ino] !=
                                disk_inode->i_nlinks) {
                                ret = -EUCLEAN;
                                goto out;
                        }
                        continue;
                }

                uint_32 expected_links = 2U + scan.child_dirs[ino];
                if (expected_links > 0xffU ||
                    disk_inode->i_nlinks != expected_links) {
                        ret = -EUCLEAN;
                        goto out;
                }
                if (ino == fsb->disk_sb.root_inode_no) {
                        if (scan.parent_refs[ino] ||
                            scan.namespace_refs[ino] ||
                            scan.dotdot[ino] != ino) {
                                ret = -EUCLEAN;
                                goto out;
                        }
                } else if (scan.parent_refs[ino] != 1 ||
                           scan.namespace_refs[ino] != 1 ||
                           scan.dotdot[ino] != scan.parent_of[ino]) {
                        ret = -EUCLEAN;
                        goto out;
                }

                uint_32 cursor = ino;
                uint_32 depth = 0;
                while (cursor != fsb->disk_sb.root_inode_no &&
                       depth++ < ninodes) {
                        cursor = scan.parent_of[cursor];
                        if (cursor >= ninodes ||
                            scan.inode_type[cursor] != FT_DIRECTORY) {
                                ret = -EUCLEAN;
                                goto out;
                        }
                }
                if (cursor != fsb->disk_sb.root_inode_no) {
                        ret = -EUCLEAN;
                        goto out;
                }
        }
out:
        frogfs_free_mount_scan(&scan);
        return ret;
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

static void frogfs_free_inode_memory(struct inode *inode)
{
        if (!inode)
                return;
        if (inode->i_private)
                kfree(inode->i_private);
        kfree(inode);
}

static void frogfs_prune_mount_scan_cache(struct super_block *sb,
                                          struct inode *root)
{
        struct list_head *pos = sb->s_inodes.next;
        while (pos != &sb->s_inodes) {
                struct list_head *next = pos->next;
                struct inode *inode =
                    container_of(pos, struct inode, i_active_node);
                if (inode != root) {
                        list_del(pos);
                        frogfs_free_inode_memory(inode);
                }
                pos = next;
        }
}

static int frogfs_commit_new_inode(struct inode *inode, uint_8 *io_buf)
{
        int ret = flush_inode(inode->i_sb, inode, io_buf);
        if (ret < 0)
                return ret;
        ret = flush_bitmap_block(inode->i_sb, INODE_BITMAP,
                                 (int_32) inode->i_num);
        if (ret == 0)
                inode->i_dirty = false;
        return ret;
}

static int frogfs_abort_new_inode(struct inode *inode,
                                  uint_8 *io_buf,
                                  bool inode_was_written)
{
        if (!inode)
                return -EINVAL;
        struct super_block *sb = inode->i_sb;
        int ret = 0;
        if (inode_was_written) {
                ret = clear_inode(sb, inode->i_num, io_buf);
                if (ret < 0) {
                        frogfs_mark_needs_fsck(sb);
                        frogfs_free_inode_memory(inode);
                        return ret;
                }
        }
        ret = free_inode_bitmap(sb, (int_32) inode->i_num);
        if (ret == 0) {
                ret = flush_bitmap_block(sb, INODE_BITMAP,
                                         (int_32) inode->i_num);
                if (ret < 0) {
                        if (flush_bitmap_block(sb, INODE_BITMAP,
                                               (int_32) inode->i_num) < 0)
                                frogfs_mark_needs_fsck(sb);
                }
        } else {
                frogfs_mark_needs_fsck(sb);
        }
        frogfs_free_inode_memory(inode);
        return ret;
}

static void frogfs_release_super(struct super_block *sb, bool sync)
{
        if (!sb || !sb->s_fs_info)
                return;
        if (sync && frogfs_sync_fs(sb, 1) < 0)
                frogfs_mark_needs_fsck(sb);

        struct list_head *pos = sb->s_inodes.next;
        while (pos != &sb->s_inodes) {
                struct list_head *next = pos->next;
                struct inode *inode =
                    container_of(pos, struct inode, i_active_node);
                list_del(pos);
                frogfs_free_inode_memory(inode);
                pos = next;
        }

        struct frogfs_super_block *fsb = sb->s_fs_info;
        if (fsb->z_bmap) {
                if (fsb->z_bmap->bits)
                        kfree(fsb->z_bmap->bits);
                kfree(fsb->z_bmap);
        }
        if (fsb->i_bmap) {
                if (fsb->i_bmap->bits)
                        kfree(fsb->i_bmap->bits);
                kfree(fsb->i_bmap);
        }
        kfree(fsb);
        sb->s_fs_info = NULL;
}

void frogfs_put_super(struct super_block *sb)
{
        frogfs_release_super(sb, true);
}

int_32 frogfs_statfs(struct super_block *sb, struct stat *buf)
{
        (void) sb;
        (void) buf;
        return -EOPNOTSUPP;
}

int_32 frogfs_remount(struct super_block *sb, uint_32 flags)
{
        (void) sb;
        (void) flags;
        return -EOPNOTSUPP;
}

static struct inode *frogfs_alloc_inode_locked(struct super_block *sb)
{
        if (frogfs_is_read_only(sb))
                return NULL;
        int inum = alloc_inode_bitmap(sb);
        if (inum < 0)
                return NULL;

        struct inode *inode = kmalloc(sizeof(*inode));
        struct frogfs_inode *disk_inode = kmalloc(sizeof(*disk_inode));
        if (!inode || !disk_inode) {
                if (inode)
                        kfree(inode);
                if (disk_inode)
                        kfree(disk_inode);
                if (free_inode_bitmap(sb, inum) < 0)
                        frogfs_mark_needs_fsck(sb);
                return NULL;
        }

        memset(inode, 0, sizeof(*inode));
        memset(disk_inode, 0, sizeof(*disk_inode));
        INIT_LIST_HEAD(&inode->i_active_node);
        inode->i_num = (uint_32) inum;
        inode->i_sb = sb;
        inode->i_op = &frog_iop;
        inode->i_fop = &frog_fop;
        inode->i_private = disk_inode;
        disk_inode->i_num = (uint_32) inum;
        return inode;
}

static void frogfs_destory_inode_locked(struct super_block *sb,
                                        struct inode *inode)
{
        (void) sb;
        if (!inode)
                return;
        frogfs_free_inode_memory(inode);
}

static void frogfs_write_inode_locked(struct inode *inode,
                                      struct writeback_control *wbc)
{
        (void) wbc;
        if (!inode || !inode->i_dirty || !inode->i_sb)
                return;
        uint_8 *buf = kmalloc(2 * ZONE_SIZE);
        if (!buf)
                return;
        if (flush_inode(inode->i_sb, inode, buf) == 0) {
                inode->i_dirty = false;
        } else {
                frogfs_mark_needs_fsck(inode->i_sb);
        }
        kfree(buf);
}

static void frogfs_evict_inode_locked(struct inode *inode)
{
        if (!inode)
                return;
        frogfs_write_inode(inode, NULL);
}

static int frogfs_sync_fs_locked(struct super_block *sb, int wait)
{
        (void) wait;
        if (!sb || !sb->s_fs_info)
                return -EINVAL;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        if (fsb->needs_fsck)
                return -EUCLEAN;
        if (frogfs_is_read_only(sb))
                return 0;

        uint_8 *buf = kmalloc(2 * ZONE_SIZE);
        if (!buf)
                return -ENOMEM;
        int ret = 0;
        struct list_head *pos;
        list_for_each (pos, &sb->s_inodes) {
                struct inode *inode =
                    container_of(pos, struct inode, i_active_node);
                if (!inode->i_dirty)
                        continue;
                ret = flush_inode(sb, inode, buf);
                if (ret < 0)
                        break;
                inode->i_dirty = false;
        }
        kfree(buf);
        if (ret < 0)
                goto io_error;

        ret = write_bitmap(sb, fsb->disk_sb.s_zmap_blk,
                           fsb->disk_sb.s_zmap_sz, fsb->z_bmap);
        if (ret < 0)
                goto io_error;
        ret = write_bitmap(sb, fsb->disk_sb.s_imap_blk,
                           fsb->disk_sb.s_imap_sz, fsb->i_bmap);
        if (ret < 0)
                goto io_error;
        return ret;

io_error:
        frogfs_mark_needs_fsck(sb);
        return ret;
}

static void frogfs_set_raw_bitmap_bit(uint_8 *bits, uint_32 bit)
{
        bits[bit / 8] |= (uint_8) (1U << (bit % 8));
}

static int frogfs_format_partition(struct block_device *bdev,
                                   struct frogfs_super_block *fsb)
{
        if (!fsb || frogfs_validate_bdev(bdev) < 0 ||
            bdev->bd_start_lba % SECTOR_PER_ZONE)
                return -EINVAL;

        uint_32 start_block = bdev->bd_start_lba / SECTOR_PER_ZONE;
        uint_32 total_blocks = bdev->bd_sec_cnt / SECTOR_PER_ZONE;
        uint_32 inode_bitmap_blocks =
            DIV_ROUND_UP(MAX_FILES_PER_PARTITION, BITS_PER_ZONE);
        uint_32 inode_table_blocks =
            DIV_ROUND_UP(sizeof(struct frogfs_inode) *
                             MAX_FILES_PER_PARTITION,
                         ZONE_SIZE);
        uint_32 fixed_blocks = 1 + inode_bitmap_blocks + inode_table_blocks;
        if (total_blocks <= fixed_blocks + 1)
                return -ENOSPC;

        uint_32 available = total_blocks - fixed_blocks;
        uint_32 zone_bitmap_blocks =
            DIV_ROUND_UP(available, BITS_PER_ZONE + 1);
        uint_32 data_blocks = available - zone_bitmap_blocks;
        if (!data_blocks ||
            zone_bitmap_blocks < DIV_ROUND_UP(data_blocks, BITS_PER_ZONE))
                return -ENOSPC;

        memset(&fsb->disk_sb, 0, sizeof(fsb->disk_sb));
        fsb->disk_sb.s_magic = FROGFS_MAGIC;
        strncpy(fsb->disk_sb.vol_name, "frogfs",
                sizeof(fsb->disk_sb.vol_name) - 1);
        fsb->disk_sb.s_ninodes = MAX_FILES_PER_PARTITION;
        fsb->disk_sb.s_inode_sz = sizeof(struct frogfs_inode);
        fsb->disk_sb.s_nzones = data_blocks;
        fsb->disk_sb.s_zone_sz = ZONE_SIZE;
        fsb->disk_sb.s_imap_blk = start_block + 1;
        fsb->disk_sb.s_imap_sz = inode_bitmap_blocks;
        fsb->disk_sb.s_zmap_blk =
            fsb->disk_sb.s_imap_blk + inode_bitmap_blocks;
        fsb->disk_sb.s_zmap_sz = zone_bitmap_blocks;
        fsb->disk_sb.s_inode_table_blk =
            fsb->disk_sb.s_zmap_blk + zone_bitmap_blocks;
        fsb->disk_sb.s_inode_table_sz = inode_table_blocks;
        fsb->disk_sb.s_data_start_blk =
            fsb->disk_sb.s_inode_table_blk + inode_table_blocks;
        fsb->disk_sb.root_inode_no = 0;
        fsb->disk_sb.dir_entry_size = sizeof(struct frogfs_dir_entry);
        fsb->disk_sb.s_log_zone_sz = 1;
        fsb->disk_sb.s_max_file_sz = MAX_FILE_SIZE;

        struct __frogfs_super_block invalid_super;
        memset(&invalid_super, 0, sizeof(invalid_super));
        int ret = frogfs_write_super_sector(bdev, &invalid_super);
        if (ret < 0)
                return ret;

        struct super_block format_sb;
        memset(&format_sb, 0, sizeof(format_sb));
        format_sb.s_bdev = bdev;
        format_sb.s_fs_info = fsb;

        uint_8 *buf = kmalloc(ZONE_SIZE);
        if (!buf)
                return -ENOMEM;

        for (uint_32 block = 0; block < inode_bitmap_blocks; block++) {
                memset(buf, 0, ZONE_SIZE);
                if (block == 0)
                        frogfs_set_raw_bitmap_bit(buf, 0);
                for (uint_32 bit = 0; bit < BITS_PER_ZONE; bit++) {
                        uint_32 inode_no = block * BITS_PER_ZONE + bit;
                        if (inode_no >= MAX_FILES_PER_PARTITION)
                                frogfs_set_raw_bitmap_bit(buf, bit);
                }
                if (write_blocks(&format_sb,
                                 fsb->disk_sb.s_imap_blk + block, 1,
                                 buf) < 0)
                        goto io_error;
        }

        for (uint_32 block = 0; block < zone_bitmap_blocks; block++) {
                memset(buf, 0, ZONE_SIZE);
                for (uint_32 bit = 0; bit < BITS_PER_ZONE; bit++) {
                        uint_32 zone = block * BITS_PER_ZONE + bit;
                        if (zone == 0 || zone >= data_blocks)
                                frogfs_set_raw_bitmap_bit(buf, bit);
                }
                if (write_blocks(&format_sb,
                                 fsb->disk_sb.s_zmap_blk + block, 1,
                                 buf) < 0)
                        goto io_error;
        }

        memset(buf, 0, ZONE_SIZE);
        for (uint_32 block = 0; block < inode_table_blocks; block++) {
                if (write_blocks(&format_sb,
                                 fsb->disk_sb.s_inode_table_blk + block, 1,
                                 buf) < 0)
                        goto io_error;
        }

        struct frogfs_inode *root_inode = (struct frogfs_inode *) buf;
        root_inode->i_num = 0;
        root_inode->i_mode = FT_DIRECTORY << 11;
        root_inode->i_size = sizeof(struct frogfs_dir_entry) * 2;
        root_inode->i_nlinks = 2;
        root_inode->i_zones[0] = fsb->disk_sb.s_data_start_blk;
        root_inode->i_blocks = 1;
        if (write_blocks(&format_sb, fsb->disk_sb.s_inode_table_blk, 1,
                         buf) < 0)
                goto io_error;

        memset(buf, 0, ZONE_SIZE);
        struct frogfs_dir_entry *dot = (struct frogfs_dir_entry *) buf;
        struct frogfs_dir_entry *dotdot = dot + 1;
        frogfs_init_dir_entry(dot, ".", 0, FT_DIRECTORY);
        frogfs_init_dir_entry(dotdot, "..", 0, FT_DIRECTORY);
        if (write_blocks(&format_sb, fsb->disk_sb.s_data_start_blk, 1,
                         buf) < 0)
                goto io_error;

        kfree(buf);
        return frogfs_write_super_sector(bdev, &fsb->disk_sb);

io_error:
        kfree(buf);
        return -EIO;
}

static struct super_block *frogfs_mount(struct fs_type *fs,
                                        int flags,
                                        const struct vfs_mount_source *source,
                                        void *data)
{
        (void) fs;
        (void) data;
        if (!source || (flags & ~FROGFS_MOUNT_FORMAT))
                return NULL;

        struct block_device *bdev = NULL;
        dev_t devno = 0;
        if (source->type == VFS_MOUNT_SOURCE_BLOCK) {
                bdev = source->value.bdev;
                if (bdev)
                        devno = bdev->bd_dev;
        } else if (source->type == VFS_MOUNT_SOURCE_PATH) {
                struct dentry *device_dentry =
                    vfs_lookup(source->value.path);
                if (!device_dentry || !device_dentry->d_inode ||
                    device_dentry->d_type != FT_BLOCK)
                        return NULL;
                devno = device_dentry->d_inode->i_dev;
                bdev = get_block_device(devno);
        } else {
                return NULL;
        }
        if (!bdev)
                return NULL;

        if (frogfs_validate_bdev(bdev) < 0)
                return NULL;

        struct super_block *sb = kmalloc(sizeof(*sb));
        struct frogfs_super_block *fsb = kmalloc(sizeof(*fsb));
        struct bitmap *inode_bitmap = kmalloc(sizeof(*inode_bitmap));
        struct bitmap *zone_bitmap = kmalloc(sizeof(*zone_bitmap));
        if (!sb || !fsb || !inode_bitmap || !zone_bitmap)
                goto alloc_error;
        memset(sb, 0, sizeof(*sb));
        memset(fsb, 0, sizeof(*fsb));
        memset(inode_bitmap, 0, sizeof(*inode_bitmap));
        memset(zone_bitmap, 0, sizeof(*zone_bitmap));

        int ret;
        if (flags & FROGFS_MOUNT_FORMAT) {
                ret = frogfs_format_partition(bdev, fsb);
                if (ret < 0)
                        goto mount_error;
        }
        if (frogfs_probe_superblock(bdev, &fsb->disk_sb) !=
            FROGFS_PROBE_VALID)
                goto mount_error;

        fsb->i_bmap = inode_bitmap;
        fsb->z_bmap = zone_bitmap;
        lock_init(&fsb->fs_lock);
        fsb->needs_fsck = false;
        sb->s_fs_info = fsb;
        sb->s_devno = devno;
        sb->s_magic = fsb->disk_sb.s_magic;
        sb->s_op = &frog_sop;
        sb->s_block_size = fsb->disk_sb.s_zone_sz;
        sb->s_bdev = bdev;
        INIT_LIST_HEAD(&sb->s_inodes);

        ret = read_bitmap(sb, fsb->disk_sb.s_imap_blk,
                          fsb->disk_sb.s_imap_sz, inode_bitmap);
        if (ret < 0)
                goto mount_error;
        ret = read_bitmap(sb, fsb->disk_sb.s_zmap_blk,
                          fsb->disk_sb.s_zmap_sz, zone_bitmap);
        if (ret < 0)
                goto mount_error;
        if (!frogfs_bitmap_bit_is_set(inode_bitmap, 0) ||
            !frogfs_bitmap_bit_is_set(zone_bitmap, 0))
                goto mount_error;

        ret = frogfs_scan_allocated_inodes(sb);
        if (ret < 0)
                goto mount_error;

        struct inode *root = geti(sb, fsb->disk_sb.root_inode_no);
        if (!root || GET_FILE_TYPE(((struct frogfs_inode *) root->i_private)
                                       ->i_mode) != FT_DIRECTORY)
                goto mount_error;
        struct frogfs_block_map root_map;
        if (frogfs_load_block_map(root, &root_map) < 0)
                goto mount_error;
        frogfs_free_block_map_buffers(&root_map);
        root->i_op = &frog_iop;
        root->i_fop = &frog_fop;
        frogfs_fill_vfs_inode(root, root->i_private);
        sb->s_root = root;
        frogfs_prune_mount_scan_cache(sb, root);
        return sb;

mount_error:
        if (sb && sb->s_fs_info) {
                frogfs_release_super(sb, false);
        } else {
                if (inode_bitmap) {
                        if (inode_bitmap->bits)
                                kfree(inode_bitmap->bits);
                        kfree(inode_bitmap);
                }
                if (zone_bitmap) {
                        if (zone_bitmap->bits)
                                kfree(zone_bitmap->bits);
                        kfree(zone_bitmap);
                }
                if (fsb)
                        kfree(fsb);
        }
        if (sb)
                kfree(sb);
        return NULL;

alloc_error:
        if (zone_bitmap)
                kfree(zone_bitmap);
        if (inode_bitmap)
                kfree(inode_bitmap);
        if (fsb)
                kfree(fsb);
        if (sb)
                kfree(sb);
        return NULL;
}

static int_32 frogfs_open_locked(struct inode *inode, struct file *file)
{
        if (!inode || !file || !inode->i_sb || !inode->i_private)
                return -EINVAL;
        struct frogfs_inode *disk_inode = inode->i_private;
        uint_32 access_mode = file->f_flag & O_ACCMODE;
        bool writable = access_mode == O_WRONLY || access_mode == O_RDWR;
        uint_32 type = GET_FILE_TYPE(disk_inode->i_mode);
        if (access_mode != O_RDONLY && !writable)
                return -EINVAL;
        if (writable && frogfs_is_read_only(inode->i_sb))
                return -EROFS;
        if ((file->f_flag & O_DIRECTORY) && type != FT_DIRECTORY)
                return -ENOTDIR;
        if (writable && type == FT_DIRECTORY)
                return -EISDIR;
        if ((file->f_flag & O_TRUNC) && !writable)
                return -EINVAL;

        unsigned long irq_flags;
        local_irq_save(irq_flags);
        if (writable && inode->i_lock) {
                local_irq_restore(irq_flags);
                return -EBUSY;
        }
        if (writable)
                inode->i_lock = true;
        local_irq_restore(irq_flags);

        if (file->f_flag & O_TRUNC) {
                int ret = frogfs_truncate_inode(inode);
                if (ret < 0) {
                        local_irq_save(irq_flags);
                        inode->i_lock = false;
                        local_irq_restore(irq_flags);
                        return ret;
                }
        }

        frogfs_fill_vfs_inode(inode, disk_inode);
        file->f_inode = inode;
        file->f_op = inode->i_fop;
        file->private_data = disk_inode;
        file->f_pos = (file->f_flag & O_APPEND) ? inode->i_size : 0;
        return 0;
}

static int_32 frogfs_close_locked(struct file *file)
{
        if (!file || !file->f_inode)
                return -EINVAL;
        struct inode *inode = file->f_inode;
        uint_32 access_mode = file->f_flag & O_ACCMODE;
        bool writable = access_mode == O_WRONLY || access_mode == O_RDWR;
        int ret = 0;
        if (writable && inode->i_dirty) {
                uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
                if (!io_buf) {
                        ret = -ENOMEM;
                } else {
                        ret = flush_inode(inode->i_sb, inode, io_buf);
                        kfree(io_buf);
                }
                if (ret < 0)
                        frogfs_mark_needs_fsck(inode->i_sb);
                else
                        inode->i_dirty = false;
        }
        unsigned long irq_flags;
        local_irq_save(irq_flags);
        if (writable)
                inode->i_lock = false;
        local_irq_restore(irq_flags);
        file->private_data = NULL;
        return ret;
}

static int_32 frogfs_read_locked(struct file *file, void *buf, uint_32 count)
{
        if (!file || !file->f_inode || !buf)
                return -EINVAL;
        if ((file->f_flag & O_ACCMODE) == O_WRONLY)
                return -EBADF;
        struct inode *inode = file->f_inode;
        struct frogfs_inode *disk_inode = inode->i_private;
        if (!disk_inode)
                return -EINVAL;
        if (GET_FILE_TYPE(disk_inode->i_mode) == FT_DIRECTORY)
                return -EISDIR;
        if (!count || file->f_pos >= disk_inode->i_size)
                return 0;

        uint_8 *io_buf = kmalloc(ZONE_SIZE);
        if (!io_buf)
                return -ENOMEM;
        uint_8 *cursor = buf;
        uint_32 bytes_read = 0;
        uint_32 readable = MIN(count, disk_inode->i_size - file->f_pos);
        int error = 0;
        while (bytes_read < readable) {
                uint_32 position = file->f_pos + bytes_read;
                uint_32 block_idx = position / ZONE_SIZE;
                uint_32 block_offset = position % ZONE_SIZE;
                uint_32 chunk =
                    MIN(ZONE_SIZE - block_offset, readable - bytes_read);
                int block_no = frogfs_get_file_block(inode, block_idx, false,
                                                      NULL);
                if (block_no < 0) {
                        error = block_no;
                        break;
                }
                if (read_blocks(inode->i_sb, (uint_32) block_no, 1,
                                io_buf) < 0) {
                        error = -EIO;
                        break;
                }
                memcpy(cursor, io_buf + block_offset, chunk);
                cursor += chunk;
                bytes_read += chunk;
        }
        file->f_pos += bytes_read;
        kfree(io_buf);
        return bytes_read ? (int_32) bytes_read : error;
}

static int_32 frogfs_write_locked(struct file *file,
                                  const void *buf,
                                  uint_32 count)
{
        if (!file || !file->f_inode || !buf)
                return -EINVAL;
        uint_32 access_mode = file->f_flag & O_ACCMODE;
        if (access_mode != O_WRONLY && access_mode != O_RDWR)
                return -EBADF;
        struct inode *inode = file->f_inode;
        struct frogfs_inode *disk_inode = inode->i_private;
        if (!disk_inode)
                return -EINVAL;
        if (GET_FILE_TYPE(disk_inode->i_mode) == FT_DIRECTORY)
                return -EISDIR;
        if (frogfs_is_read_only(inode->i_sb))
                return -EROFS;
        if (!count)
                return 0;
        if (file->f_flag & O_APPEND)
                file->f_pos = disk_inode->i_size;
        uint_32 max_file_size = frogfs_max_file_size(inode->i_sb);
        if (file->f_pos > disk_inode->i_size ||
            file->f_pos >= max_file_size)
                return file->f_pos > disk_inode->i_size ? -EINVAL : -EFBIG;

        uint_32 writable = MIN(count, max_file_size - file->f_pos);
        uint_8 *io_buf = kmalloc(2 * ZONE_SIZE);
        uint_16 *new_blocks =
            kmalloc(MAX_ZONE_COUNT * sizeof(*new_blocks));
        if (!io_buf || !new_blocks) {
                if (io_buf)
                        kfree(io_buf);
                if (new_blocks)
                        kfree(new_blocks);
                return -ENOMEM;
        }
        struct frogfs_inode old_inode = *disk_inode;
        uint_32 old_pos = file->f_pos;
        uint_32 new_block_count = 0;
        const uint_8 *cursor = buf;
        uint_32 bytes_written = 0;
        int error = 0;
        while (bytes_written < writable) {
                uint_32 position = file->f_pos + bytes_written;
                uint_32 block_idx = position / ZONE_SIZE;
                uint_32 block_offset = position % ZONE_SIZE;
                uint_32 chunk = MIN(ZONE_SIZE - block_offset,
                                    writable - bytes_written);
                bool allocated = false;
                int block_no = frogfs_get_file_block(inode, block_idx, true,
                                                      &allocated);
                if (block_no < 0) {
                        error = block_no;
                        break;
                }

                if (block_offset || chunk < ZONE_SIZE) {
                        memset(io_buf, 0, ZONE_SIZE);
                        if (!allocated &&
                            read_blocks(inode->i_sb, (uint_32) block_no, 1,
                                        io_buf) < 0) {
                                error = -EIO;
                                break;
                        }
                        memcpy(io_buf + block_offset, cursor, chunk);
                        error = write_blocks(inode->i_sb,
                                             (uint_32) block_no, 1, io_buf);
                } else {
                        error = write_blocks(inode->i_sb,
                                             (uint_32) block_no, 1, cursor);
                }
                if (error < 0) {
                        if (allocated &&
                            frogfs_unmap_new_file_block(
                                inode, block_idx, &old_inode) < 0)
                                frogfs_mark_needs_fsck(inode->i_sb);
                        break;
                }
                if (allocated)
                        new_blocks[new_block_count++] = (uint_16) block_idx;
                cursor += chunk;
                bytes_written += chunk;
        }

        if (bytes_written) {
                file->f_pos += bytes_written;
                bool metadata_changed =
                    file->f_pos > old_inode.i_size || new_block_count != 0;
                if (file->f_pos > disk_inode->i_size)
                        disk_inode->i_size = file->f_pos;
                frogfs_fill_vfs_inode(inode, disk_inode);
                if (!metadata_changed) {
                        kfree(new_blocks);
                        kfree(io_buf);
                        return (int_32) bytes_written;
                }
                inode->i_dirty = true;
                int flush_ret = flush_inode(inode->i_sb, inode, io_buf);
                if (flush_ret < 0) {
                        while (new_block_count) {
                                uint_32 block_idx =
                                    new_blocks[--new_block_count];
                                if (frogfs_unmap_new_file_block(
                                        inode, block_idx, &old_inode) < 0)
                                        frogfs_mark_needs_fsck(inode->i_sb);
                        }
                        frogfs_restore_inode_state(inode, &old_inode,
                                                   io_buf);
                        file->f_pos = old_pos;
                        kfree(new_blocks);
                        kfree(io_buf);
                        return flush_ret;
                }
                inode->i_dirty = false;
        }
        kfree(new_blocks);
        kfree(io_buf);
        return bytes_written ? (int_32) bytes_written : error;
}

static int_32 frogfs_lseek_locked(struct file *file,
                                  int_32 offset,
                                  uint_8 whence)
{
        if (!file || !file->f_inode || !file->f_inode->i_private)
                return -EINVAL;
        long long base;
        switch (whence) {
        case SEEK_SET:
                base = 0;
                break;
        case SEEK_CUR:
                base = file->f_pos;
                break;
        case SEEK_END:
                base = file->f_inode->i_size;
                break;
        default:
                return -EINVAL;
        }
        long long position = base + (long long) offset;
        if (position < 0 || position > file->f_inode->i_size ||
            position > frogfs_max_file_size(file->f_inode->i_sb))
                return -EINVAL;
        file->f_pos = (uint_32) position;
        return (int_32) file->f_pos;
}

static int frogfs_validate_create_target(struct inode *dir,
                                         struct dentry *target)
{
        if (!dir || !target || !target->d_name || !dir->i_sb ||
            !dir->i_private)
                return -EINVAL;
        if (GET_FILE_TYPE(((struct frogfs_inode *) dir->i_private)->i_mode) !=
            FT_DIRECTORY)
                return -ENOTDIR;
        if (frogfs_is_read_only(dir->i_sb))
                return -EROFS;
        int name_len = frogfs_bounded_name_length(target->d_name);
        if (name_len <= 0)
                return name_len < 0 ? name_len : -EINVAL;
        for (int idx = 0; idx < name_len; idx++) {
                if (target->d_name[idx] == '/')
                        return -EINVAL;
        }
        if ((name_len == 1 && target->d_name[0] == '.') ||
            (name_len == 2 && target->d_name[0] == '.' &&
             target->d_name[1] == '.'))
                return -EINVAL;
        int existing = frogfs_lookup_dir_entry(dir, target->d_name, NULL);
        if (existing >= 0)
                return -EEXIST;
        return existing == -ENOENT ? 0 : existing;
}

static void frogfs_publish_new_inode(struct inode *dir,
                                     struct dentry *target,
                                     struct inode *inode,
                                     enum file_type type)
{
        target->d_inode = inode;
        target->d_type = type;
        target->d_sb = dir->i_sb;
        target->d_mounted = false;
        INIT_LIST_HEAD(&target->d_subdirs);
        list_add_tail(&inode->i_active_node, &dir->i_sb->s_inodes);
}

static int_32 frogfs_create_locked(struct inode *dir,
                                   struct dentry *target,
                                   uint_32 mode)
{
        if (dir && frogfs_is_read_only(dir->i_sb))
                return -EROFS;
        int ret = frogfs_validate_create_target(dir, target);
        if (ret < 0)
                return ret;
        struct inode *inode = frogfs_alloc_inode(dir->i_sb);
        if (!inode)
                return -ENOSPC;
        struct frogfs_inode *disk_inode = inode->i_private;
        disk_inode->i_mode = frogfs_regular_mode(mode);
        if (GET_FILE_TYPE(disk_inode->i_mode) != FT_REGULAR)
                disk_inode->i_mode = FT_REGULAR << 11;
        disk_inode->i_nlinks = 1;
        frogfs_fill_vfs_inode(inode, disk_inode);

        uint_8 *io_buf = kmalloc(3 * ZONE_SIZE);
        if (!io_buf) {
                frogfs_abort_new_inode(inode, NULL, false);
                return -ENOMEM;
        }
        ret = frogfs_commit_new_inode(inode, io_buf);
        if (ret < 0) {
                frogfs_abort_new_inode(inode, io_buf, true);
                kfree(io_buf);
                return ret;
        }

        struct frogfs_dir_entry entry;
        frogfs_init_dir_entry(&entry, target->d_name, inode->i_num,
                              FT_REGULAR);
        ret = frogfs_add_dir_entry(dir, &entry, io_buf);
        if (ret < 0) {
                frogfs_abort_new_inode(inode, io_buf, true);
                kfree(io_buf);
                return ret;
        }
        frogfs_publish_new_inode(dir, target, inode, FT_REGULAR);
        kfree(io_buf);
        return 0;
}

static int_32 frogfs_mkdir_locked(struct inode *dir,
                                  struct dentry *target,
                                  uint_32 mode)
{
        if (dir && frogfs_is_read_only(dir->i_sb))
                return -EROFS;
        int ret = frogfs_validate_create_target(dir, target);
        if (ret < 0)
                return ret;
        struct frogfs_inode *parent_disk_inode = dir->i_private;
        if (parent_disk_inode->i_nlinks == 0xffU)
                return -EMLINK;
        struct inode *inode = frogfs_alloc_inode(dir->i_sb);
        if (!inode)
                return -ENOSPC;
        uint_8 *io_buf = kmalloc(3 * ZONE_SIZE);
        if (!io_buf) {
                frogfs_abort_new_inode(inode, NULL, false);
                return -ENOMEM;
        }

        int block_no = frogfs_alloc_zone_block(dir->i_sb);
        if (block_no < 0) {
                frogfs_abort_new_inode(inode, io_buf, false);
                kfree(io_buf);
                return block_no;
        }
        struct frogfs_inode *disk_inode = inode->i_private;
        disk_inode->i_mode = frogfs_directory_mode(mode);
        disk_inode->i_nlinks = 2;
        disk_inode->i_zones[0] = (uint_32) block_no;
        disk_inode->i_blocks = 1;
        disk_inode->i_size = 2 * frogfs_dir_entry_size(dir->i_sb);
        frogfs_fill_vfs_inode(inode, disk_inode);

        memset(io_buf, 0, ZONE_SIZE);
        struct frogfs_dir_entry *dot = (struct frogfs_dir_entry *) io_buf;
        struct frogfs_dir_entry *dotdot = dot + 1;
        frogfs_init_dir_entry(dot, ".", inode->i_num, FT_DIRECTORY);
        frogfs_init_dir_entry(dotdot, "..", dir->i_num, FT_DIRECTORY);
        ret = write_blocks(dir->i_sb, (uint_32) block_no, 1, io_buf);
        if (ret < 0)
                goto mkdir_abort_uncommitted;
        ret = frogfs_commit_new_inode(inode, io_buf);
        if (ret < 0)
                goto mkdir_abort_written;

        struct frogfs_dir_entry entry;
        frogfs_init_dir_entry(&entry, target->d_name, inode->i_num,
                              FT_DIRECTORY);
        struct frogfs_inode old_parent_inode = *parent_disk_inode;
        ret = frogfs_add_dir_entry(dir, &entry, io_buf);
        if (ret < 0)
                goto mkdir_abort_written;

        parent_disk_inode->i_nlinks++;
        frogfs_fill_vfs_inode(dir, parent_disk_inode);
        ret = flush_inode(dir->i_sb, dir, io_buf);
        if (ret < 0) {
                frogfs_rollback_added_dir_entry(
                    dir, target->d_name, &old_parent_inode, io_buf);
                goto mkdir_abort_written;
        }
        dir->i_dirty = false;
        frogfs_publish_new_inode(dir, target, inode, FT_DIRECTORY);
        kfree(io_buf);
        return 0;

mkdir_abort_written:
        if (frogfs_abort_new_inode(inode, io_buf, true) == 0) {
                if (frogfs_free_zone_block(dir->i_sb,
                                           (uint_32) block_no) < 0)
                        frogfs_mark_needs_fsck(dir->i_sb);
        }
        kfree(io_buf);
        return ret;
mkdir_abort_uncommitted:
        frogfs_abort_new_inode(inode, io_buf, false);
        if (frogfs_free_zone_block(dir->i_sb, (uint_32) block_no) < 0)
                frogfs_mark_needs_fsck(dir->i_sb);
        kfree(io_buf);
        return ret;
}

static int frogfs_directory_is_empty(struct inode *inode,
                                     uint_32 parent_inode_no,
                                     uint_8 *io_buf)
{
        struct frogfs_inode *disk_inode = inode->i_private;
        uint_32 entry_size = frogfs_dir_entry_size(inode->i_sb);
        uint_32 entries_per_zone = ZONE_SIZE / entry_size;
        uint_32 entry_count = disk_inode->i_size / entry_size;
        uint_32 loaded_block = 0xffffffffU;
        uint_32 dot_count = 0;
        uint_32 dotdot_count = 0;
        for (uint_32 idx = 0; idx < entry_count; idx++) {
                uint_32 file_block = idx / entries_per_zone;
                if (file_block != loaded_block) {
                        int block_no = frogfs_get_file_block(
                            inode, file_block, false, NULL);
                        if (block_no < 0)
                                return block_no;
                        if (read_blocks(inode->i_sb, (uint_32) block_no, 1,
                                        io_buf) < 0)
                                return -EIO;
                        loaded_block = file_block;
                }
                struct frogfs_dir_entry *entry =
                    (struct frogfs_dir_entry *)
                        (io_buf + (idx % entries_per_zone) * entry_size);
                if (!frogfs_dir_entry_is_valid(inode->i_sb, entry))
                        return -EUCLEAN;
                if (frogfs_dir_entry_is_empty(entry))
                        continue;
                if (frogfs_dir_entry_name_eq(entry, ".")) {
                        if (++dot_count != 1 || entry->i_no != inode->i_num ||
                            entry->f_type != FT_DIRECTORY)
                                return -EUCLEAN;
                        continue;
                }
                if (frogfs_dir_entry_name_eq(entry, "..")) {
                        if (++dotdot_count != 1 ||
                            entry->i_no != parent_inode_no ||
                            entry->f_type != FT_DIRECTORY)
                                return -EUCLEAN;
                        continue;
                }
                return -ENOTEMPTY;
        }
        return dot_count == 1 && dotdot_count == 1 ? 0 : -EUCLEAN;
}

static int frogfs_restore_removed_entry(struct inode *dir,
                                        const char *name,
                                        uint_32 inode_no,
                                        enum file_type type,
                                        uint_8 *io_buf)
{
        struct frogfs_dir_entry entry;
        frogfs_init_dir_entry(&entry, name, inode_no, type);
        return frogfs_add_dir_entry(dir, &entry, io_buf);
}

static int frogfs_remove_inode(struct inode *dir,
                               struct dentry *target,
                               enum file_type type,
                               uint_8 *io_buf,
                               struct frogfs_block_map *map)
{
        if (frogfs_is_read_only(dir->i_sb))
                return -EROFS;
        struct inode *inode = target->d_inode;
        struct frogfs_inode *disk_inode = inode->i_private;
        struct frogfs_inode saved_inode = *disk_inode;
        struct frogfs_inode *parent_inode = dir->i_private;
        struct frogfs_inode saved_parent_inode = *parent_inode;
        bool parent_write_attempted = false;
        bool inode_write_attempted = false;
        bool inode_bitmap_cleared = false;

        int ret = frogfs_remove_dir_entry(dir, target->d_name, io_buf);
        if (ret < 0)
                return ret;
        if (type == FT_DIRECTORY) {
                if (parent_inode->i_nlinks)
                        parent_inode->i_nlinks--;
                frogfs_fill_vfs_inode(dir, parent_inode);
                parent_write_attempted = true;
                ret = flush_inode(dir->i_sb, dir, io_buf);
                if (ret < 0)
                        goto rollback;
                dir->i_dirty = false;
        }

        inode_write_attempted = true;
        ret = clear_inode(dir->i_sb, inode->i_num, io_buf);
        if (ret < 0)
                goto rollback;
        ret = free_inode_bitmap(dir->i_sb, (int_32) inode->i_num);
        if (ret < 0)
                goto rollback;
        inode_bitmap_cleared = true;
        ret = flush_bitmap_block(dir->i_sb, INODE_BITMAP,
                                 (int_32) inode->i_num);
        if (ret < 0)
                goto rollback;

        ret = frogfs_release_block_map(dir->i_sb, &saved_inode, map);
        if (ret < 0)
                goto rollback;
        list_del(&inode->i_active_node);
        frogfs_free_inode_memory(inode);
        target->d_inode = NULL;
        return 0;

rollback: {
        bool rollback_failed = false;
        if (inode_bitmap_cleared) {
                struct frogfs_super_block *fsb = dir->i_sb->s_fs_info;
                set_value_bitmap(fsb->i_bmap, inode->i_num, 1);
                if (flush_bitmap_block(dir->i_sb, INODE_BITMAP,
                                       (int_32) inode->i_num) < 0)
                        rollback_failed = true;
        }
        if (inode_write_attempted &&
            frogfs_restore_inode_state(inode, &saved_inode, io_buf) < 0)
                rollback_failed = true;
        if (parent_write_attempted &&
            frogfs_restore_inode_state(dir, &saved_parent_inode, io_buf) < 0)
                rollback_failed = true;
        if (frogfs_restore_removed_entry(dir, target->d_name, inode->i_num,
                                         type, io_buf) < 0)
                rollback_failed = true;
        if (rollback_failed)
                frogfs_mark_needs_fsck(dir->i_sb);
        return ret;
}
}

static int_32 frogfs_rmdir_locked(struct inode *dir, struct dentry *target)
{
        if (dir && frogfs_is_read_only(dir->i_sb))
                return -EROFS;
        if (!dir || !target || !target->d_inode || !target->d_name ||
            !dir->i_sb || target->d_inode->i_sb != dir->i_sb)
                return -EINVAL;
        struct frogfs_inode *disk_inode = target->d_inode->i_private;
        if (!disk_inode || GET_FILE_TYPE(disk_inode->i_mode) != FT_DIRECTORY)
                return -ENOTDIR;

        struct frogfs_block_map map;
        int ret = frogfs_load_block_map(target->d_inode, &map);
        if (ret < 0)
                return ret;
        uint_8 *io_buf = kmalloc(3 * ZONE_SIZE);
        if (!io_buf) {
                frogfs_free_block_map_buffers(&map);
                return -ENOMEM;
        }
        ret = frogfs_directory_is_empty(target->d_inode, dir->i_num, io_buf);
        if (ret == 0)
                ret = frogfs_remove_inode(dir, target, FT_DIRECTORY, io_buf,
                                          &map);
        kfree(io_buf);
        frogfs_free_block_map_buffers(&map);
        return ret;
}

static int_32 frogfs_unlink_locked(struct inode *dir, struct dentry *target)
{
        if (dir && frogfs_is_read_only(dir->i_sb))
                return -EROFS;
        if (!dir || !target || !target->d_inode || !target->d_name ||
            !dir->i_sb || target->d_inode->i_sb != dir->i_sb)
                return -EINVAL;
        struct frogfs_inode *disk_inode = target->d_inode->i_private;
        if (!disk_inode)
                return -EINVAL;
        if (GET_FILE_TYPE(disk_inode->i_mode) == FT_DIRECTORY)
                return -EISDIR;

        struct frogfs_block_map map;
        int ret = frogfs_load_block_map(target->d_inode, &map);
        if (ret < 0)
                return ret;
        uint_8 *io_buf = kmalloc(3 * ZONE_SIZE);
        if (!io_buf) {
                frogfs_free_block_map_buffers(&map);
                return -ENOMEM;
        }
        ret = frogfs_remove_inode(dir, target, FT_REGULAR, io_buf, &map);
        kfree(io_buf);
        frogfs_free_block_map_buffers(&map);
        return ret;
}

static struct dentry *frogfs_lookup_locked(struct inode *dir,
                                           struct dentry *target)
{
        if (!dir || !target || !target->d_name || !dir->i_sb ||
            !dir->i_private)
                return NULL;
        enum file_type entry_type = FT_UNKOWN;
        int inode_no =
            frogfs_lookup_dir_entry(dir, target->d_name, &entry_type);
        if (inode_no < 0) {
                if (inode_no == -EUCLEAN)
                        frogfs_mark_needs_fsck(dir->i_sb);
                return NULL;
        }
        struct inode *inode = geti(dir->i_sb, (uint_32) inode_no);
        if (!inode)
                return NULL;
        struct frogfs_block_map map;
        int ret = frogfs_load_block_map(inode, &map);
        if (ret < 0) {
                if (ret == -EUCLEAN)
                        frogfs_mark_needs_fsck(dir->i_sb);
                return NULL;
        }
        frogfs_free_block_map_buffers(&map);
        inode->i_op = &frog_iop;
        inode->i_fop = &frog_fop;
        frogfs_fill_vfs_inode(inode, inode->i_private);

        struct frogfs_inode *disk_inode = inode->i_private;
        uint_32 inode_type = GET_FILE_TYPE(disk_inode->i_mode);
        if ((entry_type == FT_DIRECTORY && inode_type != FT_DIRECTORY) ||
            (entry_type == FT_REGULAR && inode_type != FT_REGULAR))
                return NULL;
        target->d_inode = inode;
        target->d_sb = dir->i_sb;
        target->d_type = GET_FILE_TYPE(disk_inode->i_mode) == FT_DIRECTORY
                             ? FT_DIRECTORY
                             : FT_REGULAR;
        target->d_mounted = false;
        INIT_LIST_HEAD(&target->d_subdirs);
        dentry_add_child(target->d_parent, target);
        return target;
}

struct inode *frogfs_alloc_inode(struct super_block *sb)
{
        struct frogfs_super_block *fsb = frogfs_lock_instance(sb);
        if (!fsb)
                return NULL;
        struct inode *inode = frogfs_alloc_inode_locked(sb);
        frogfs_unlock_instance(fsb);
        return inode;
}

void frogfs_destory_inode(struct super_block *sb, struct inode *inode)
{
        struct frogfs_super_block *fsb = frogfs_lock_instance(sb);
        if (!fsb)
                return;
        frogfs_destory_inode_locked(sb, inode);
        frogfs_unlock_instance(fsb);
}

void frogfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(inode ? inode->i_sb : NULL);
        if (!fsb)
                return;
        frogfs_write_inode_locked(inode, wbc);
        frogfs_unlock_instance(fsb);
}

void frogfs_evict_inode(struct inode *inode)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(inode ? inode->i_sb : NULL);
        if (!fsb)
                return;
        frogfs_evict_inode_locked(inode);
        frogfs_unlock_instance(fsb);
}

int frogfs_sync_fs(struct super_block *sb, int wait)
{
        struct frogfs_super_block *fsb = frogfs_lock_instance(sb);
        if (!fsb)
                return -EINVAL;
        int ret = frogfs_sync_fs_locked(sb, wait);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_open(struct inode *inode, struct file *file)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(inode ? inode->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_open_locked(inode, file);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_close(struct file *file)
{
        struct frogfs_super_block *fsb = frogfs_lock_instance(
            file && file->f_inode ? file->f_inode->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_close_locked(file);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_read(struct file *file, void *buf, uint_32 count)
{
        struct frogfs_super_block *fsb = frogfs_lock_instance(
            file && file->f_inode ? file->f_inode->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_read_locked(file, buf, count);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_write(struct file *file, const void *buf, uint_32 count)
{
        struct frogfs_super_block *fsb = frogfs_lock_instance(
            file && file->f_inode ? file->f_inode->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_write_locked(file, buf, count);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_lseek(struct file *file, int_32 offset, uint_8 whence)
{
        struct frogfs_super_block *fsb = frogfs_lock_instance(
            file && file->f_inode ? file->f_inode->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_lseek_locked(file, offset, whence);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_create(struct inode *dir, struct dentry *target, uint_32 mode)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(dir ? dir->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_create_locked(dir, target, mode);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_mkdir(struct inode *dir, struct dentry *target, uint_32 mode)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(dir ? dir->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_mkdir_locked(dir, target, mode);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_rmdir(struct inode *dir, struct dentry *target)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(dir ? dir->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_rmdir_locked(dir, target);
        frogfs_unlock_instance(fsb);
        return ret;
}

int_32 frogfs_unlink(struct inode *dir, struct dentry *target)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(dir ? dir->i_sb : NULL);
        if (!fsb)
                return -EINVAL;
        int_32 ret = frogfs_unlink_locked(dir, target);
        frogfs_unlock_instance(fsb);
        return ret;
}

struct dentry *frogfs_lookup(struct inode *dir, struct dentry *target)
{
        struct frogfs_super_block *fsb =
            frogfs_lock_instance(dir ? dir->i_sb : NULL);
        if (!fsb)
                return NULL;
        struct dentry *ret = frogfs_lookup_locked(dir, target);
        frogfs_unlock_instance(fsb);
        return ret;
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

int_32 frogfs_mmap(struct file *file, struct vm_area *vma)
{
        (void) file;
        (void) vma;
        return -EOPNOTSUPP;
}

int frogfs_init(void)
{
        int ret = register_fs(&frogfs_type);
        if (ret < 0)
                return ret;
        frogfs_registered = true;
        return 0;
}

int frogfs_init_rollback(void)
{
        if (frogfs_registered) {
                int ret = unregister_fs(&frogfs_type);
                if (ret < 0)
                        return ret;
                frogfs_registered = false;
        }
        return 0;
}

bool frogfs_super_is_read_only(const struct super_block *sb)
{
        return frogfs_is_read_only(sb);
}
