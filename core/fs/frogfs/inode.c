#include <frog/errno.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/vfs.h>
#include "ffs_utils.h"
#include "frogfs.h"
#include "inode.h"
#include "super_block.h"

struct inode_position {
        bool crosses_zone;
        uint_32 start_block;
        uint_32 offset;
};

static int locate_inode(struct super_block *sb,
                        uint_32 inode_nr,
                        struct inode_position *position)
{
        if (!sb || !sb->s_fs_info || !position)
                return -EINVAL;

        struct frogfs_super_block *fsb = sb->s_fs_info;
        if (inode_nr >= fsb->disk_sb.s_ninodes ||
            fsb->disk_sb.s_inode_sz != sizeof(struct frogfs_inode))
                return -EINVAL;

        unsigned long long byte_offset =
            (unsigned long long) inode_nr * sizeof(struct frogfs_inode);
        uint_32 block_offset = (uint_32) (byte_offset / ZONE_SIZE);
        uint_32 offset = (uint_32) (byte_offset % ZONE_SIZE);
        uint_32 block_count =
            offset + sizeof(struct frogfs_inode) > ZONE_SIZE ? 2 : 1;

        if (block_offset >= fsb->disk_sb.s_inode_table_sz ||
            block_count > fsb->disk_sb.s_inode_table_sz - block_offset)
                return -EUCLEAN;

        position->start_block =
            fsb->disk_sb.s_inode_table_blk + block_offset;
        position->offset = offset;
        position->crosses_zone = block_count == 2;
        return 0;
}

static struct inode *find_open_inode(struct list_head *list, uint_32 inode_nr)
{
        struct list_head *pos;
        list_for_each (pos, list) {
                struct inode *inode =
                    container_of(pos, struct inode, i_active_node);
                if (inode->i_num == inode_nr)
                        return inode;
        }
        return NULL;
}

static bool bitmap_bit_is_set(struct bitmap *bmap, uint_32 bit)
{
        return bmap && bmap->bits && bit < bmap->map_bytes_length * 8 &&
               get_value_bitmap(bmap, bit);
}

static bool inode_zone_is_valid(struct frogfs_super_block *fsb,
                                uint_32 block_no)
{
        if (!block_no)
                return true;
        if (block_no < fsb->disk_sb.s_data_start_blk ||
            block_no - fsb->disk_sb.s_data_start_blk >=
                fsb->disk_sb.s_nzones)
                return false;

        uint_32 zone_idx = block_no - fsb->disk_sb.s_data_start_blk;
        return bitmap_bit_is_set(fsb->z_bmap, zone_idx);
}

static bool disk_inode_is_valid(struct frogfs_super_block *fsb,
                                struct frogfs_inode *inode,
                                uint_32 inode_nr)
{
        if (inode->i_num != inode_nr)
                return false;

        uint_32 type = GET_FILE_TYPE(inode->i_mode);
        if (type != FT_REGULAR && type != FT_DIRECTORY)
                return false;
        if ((type == FT_REGULAR && inode->i_nlinks == 0) ||
            (type == FT_DIRECTORY && inode->i_nlinks < 2))
                return false;
        if (inode->i_size > fsb->disk_sb.s_max_file_sz ||
            inode->i_size > MAX_FILE_SIZE ||
            inode->i_blocks >
                MAX_ZONE_COUNT + (ZONE_IDX_MAX - 11))
                return false;
        if (type == FT_DIRECTORY &&
            (inode->i_size < 2 * fsb->disk_sb.dir_entry_size ||
             inode->i_size % fsb->disk_sb.dir_entry_size))
                return false;

        for (uint_32 slot = 0; slot < ZONE_IDX_MAX; slot++) {
                if (!inode_zone_is_valid(fsb, inode->i_zones[slot]))
                        return false;
        }

        uint_32 required_blocks = DIV_ROUND_UP(inode->i_size, ZONE_SIZE);
        uint_32 direct_required = required_blocks < 11 ? required_blocks : 11;
        for (uint_32 slot = 0; slot < direct_required; slot++) {
                if (!inode->i_zones[slot])
                        return false;
        }

        uint_32 remaining = required_blocks - direct_required;
        for (uint_32 table = 0; remaining; table++) {
                if (table >= ZONE_IDX_MAX - 11 ||
                    !inode->i_zones[11 + table])
                        return false;
                uint_32 covered = remaining < ZONE_SIZE / sizeof(uint_32)
                                      ? remaining
                                      : ZONE_SIZE / sizeof(uint_32);
                remaining -= covered;
        }

        return true;
}

struct inode *geti(struct super_block *sb, uint_32 inode_nr)
{
        if (!sb || !sb->s_fs_info)
                return NULL;

        struct frogfs_super_block *fsb = sb->s_fs_info;
        if (inode_nr >= fsb->disk_sb.s_ninodes ||
            !bitmap_bit_is_set(fsb->i_bmap, inode_nr))
                return NULL;

        struct inode *cached = find_open_inode(&sb->s_inodes, inode_nr);
        if (cached)
                return cached;

        struct inode_position position;
        if (locate_inode(sb, inode_nr, &position) < 0)
                return NULL;

        struct inode *inode = kmalloc(sizeof(*inode));
        struct frogfs_inode *disk_inode = kmalloc(sizeof(*disk_inode));
        uint_32 read_blocks_count = position.crosses_zone ? 2 : 1;
        uint_8 *buf = kmalloc(read_blocks_count * ZONE_SIZE);
        if (!inode || !disk_inode || !buf) {
                if (inode)
                        kfree(inode);
                if (disk_inode)
                        kfree(disk_inode);
                if (buf)
                        kfree(buf);
                return NULL;
        }

        int ret = read_blocks(sb, position.start_block, read_blocks_count, buf);
        if (ret < 0) {
                kfree(buf);
                kfree(disk_inode);
                kfree(inode);
                return NULL;
        }

        memcpy(disk_inode, buf + position.offset, sizeof(*disk_inode));
        kfree(buf);
        if (!disk_inode_is_valid(fsb, disk_inode, inode_nr)) {
                kfree(disk_inode);
                kfree(inode);
                return NULL;
        }

        memset(inode, 0, sizeof(*inode));
        inode->i_num = inode_nr;
        inode->i_sb = sb;
        inode->i_private = disk_inode;
        INIT_LIST_HEAD(&inode->i_active_node);
        list_add_tail(&inode->i_active_node, &sb->s_inodes);
        return inode;
}

void new_inode(uint_32 inode_nr, struct inode *inode)
{
        if (!inode || !inode->i_private)
                return;
        struct frogfs_inode *disk_inode = inode->i_private;
        struct super_block *sb = inode->i_sb;
        memset(disk_inode, 0, sizeof(*disk_inode));
        memset(inode, 0, sizeof(*inode));
        INIT_LIST_HEAD(&inode->i_active_node);
        inode->i_num = inode_nr;
        inode->i_sb = sb;
        inode->i_private = disk_inode;
        disk_inode->i_num = inode_nr;
}

int flush_inode(struct super_block *sb, struct inode *inode, void *io_buf)
{
        if (!sb || !inode || !inode->i_private || !io_buf ||
            inode->i_sb != sb)
                return -EINVAL;

        struct inode_position position;
        int ret = locate_inode(sb, inode->i_num, &position);
        if (ret < 0)
                return ret;

        struct frogfs_inode *disk_inode = inode->i_private;
        disk_inode->i_num = inode->i_num;
        disk_inode->i_gid = (uint_8) inode->i_gid;
        disk_inode->i_uid = (uint_16) inode->i_uid;
        disk_inode->i_atime = inode->i_atime;
        disk_inode->i_ctime = inode->i_ctime;
        disk_inode->i_mtime = inode->i_mtime;
        disk_inode->i_mode = inode->i_mode;
        disk_inode->i_size = inode->i_size;
        disk_inode->i_nlinks = (uint_8) inode->i_nlink;
        disk_inode->i_dev = (uint_16) inode->i_dev;

        uint_32 block_count = position.crosses_zone ? 2 : 1;
        uint_8 *buf = io_buf;
        ret = read_blocks(sb, position.start_block, block_count, buf);
        if (ret < 0)
                return ret;
        memcpy(buf + position.offset, disk_inode, sizeof(*disk_inode));
        return write_blocks(sb, position.start_block, block_count, buf);
}

int clear_inode(struct super_block *sb, uint_32 inode_nr, void *io_buf)
{
        if (!sb || !io_buf)
                return -EINVAL;

        struct inode_position position;
        int ret = locate_inode(sb, inode_nr, &position);
        if (ret < 0)
                return ret;

        uint_32 block_count = position.crosses_zone ? 2 : 1;
        uint_8 *buf = io_buf;
        ret = read_blocks(sb, position.start_block, block_count, buf);
        if (ret < 0)
                return ret;
        memset(buf + position.offset, 0, sizeof(struct frogfs_inode));
        return write_blocks(sb, position.start_block, block_count, buf);
}
