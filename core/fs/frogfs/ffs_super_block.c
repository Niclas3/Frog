#include <frog/bitmap.h>
#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/vfs.h>
#include "ffs_utils.h"
#include "frogfs.h"
#include "super_block.h"

static int_32 alloc_bitmap_bit(struct bitmap *bmap, uint_32 bit_limit)
{
        if (!bmap || !bmap->bits || bit_limit == 0 ||
            bit_limit > bmap->map_bytes_length * 8)
                return -EINVAL;

        unsigned long flags;
        local_irq_save(flags);
        for (uint_32 bit = 0; bit < bit_limit; bit++) {
                if (!get_value_bitmap(bmap, bit)) {
                        set_value_bitmap(bmap, bit, 1);
                        local_irq_restore(flags);
                        return (int_32) bit;
                }
        }
        local_irq_restore(flags);
        return -ENOSPC;
}

static int_32 free_bitmap_bit(struct bitmap *bmap,
                              uint_32 bit_limit,
                              int_32 index)
{
        if (!bmap || !bmap->bits || index <= 0 ||
            (uint_32) index >= bit_limit ||
            bit_limit > bmap->map_bytes_length * 8)
                return -EINVAL;

        unsigned long flags;
        local_irq_save(flags);
        if (!get_value_bitmap(bmap, (uint_32) index)) {
                local_irq_restore(flags);
                return -EINVAL;
        }
        set_value_bitmap(bmap, (uint_32) index, 0);
        local_irq_restore(flags);
        return 0;
}

int_32 alloc_inode_bitmap(struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return -EINVAL;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        return alloc_bitmap_bit(fsb->i_bmap, fsb->disk_sb.s_ninodes);
}

int_32 free_inode_bitmap(struct super_block *sb, int_32 index)
{
        if (!sb || !sb->s_fs_info)
                return -EINVAL;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        return free_bitmap_bit(fsb->i_bmap, fsb->disk_sb.s_ninodes, index);
}

int_32 alloc_zone_bitmap(struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return -EINVAL;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        return alloc_bitmap_bit(fsb->z_bmap, fsb->disk_sb.s_nzones);
}

int_32 free_znode_bitmap(struct super_block *sb, int_32 index)
{
        if (!sb || !sb->s_fs_info)
                return -EINVAL;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        return free_bitmap_bit(fsb->z_bmap, fsb->disk_sb.s_nzones, index);
}

int flush_bitmap_block(struct super_block *sb,
                       enum frogfs_bmap_t b_type,
                       int_32 bit_idx)
{
        if (!sb || !sb->s_fs_info || bit_idx < 0)
                return -EINVAL;

        struct frogfs_super_block *fsb = sb->s_fs_info;
        struct bitmap *bmap;
        uint_32 bitmap_start;
        uint_32 bitmap_blocks;
        uint_32 bit_limit;

        if (b_type == INODE_BITMAP) {
                bmap = fsb->i_bmap;
                bitmap_start = fsb->disk_sb.s_imap_blk;
                bitmap_blocks = fsb->disk_sb.s_imap_sz;
                bit_limit = fsb->disk_sb.s_ninodes;
        } else if (b_type == ZONE_BITMAP) {
                bmap = fsb->z_bmap;
                bitmap_start = fsb->disk_sb.s_zmap_blk;
                bitmap_blocks = fsb->disk_sb.s_zmap_sz;
                bit_limit = fsb->disk_sb.s_nzones;
        } else {
                return -EINVAL;
        }

        if (!bmap || !bmap->bits || (uint_32) bit_idx >= bit_limit)
                return -EINVAL;

        uint_32 block_offset = (uint_32) bit_idx / BITS_PER_ZONE;
        uint_32 byte_offset = block_offset * ZONE_SIZE;
        if (block_offset >= bitmap_blocks ||
            byte_offset + ZONE_SIZE > bmap->map_bytes_length)
                return -EIO;

        return write_blocks(sb, bitmap_start + block_offset, 1,
                            bmap->bits + byte_offset);
}

int read_bitmap(struct super_block *sb,
                uint_32 blk_start,
                uint_32 blk_size,
                struct bitmap *bmap)
{
        if (!sb || !bmap || blk_size == 0 ||
            blk_size > 0xffffffffU / ZONE_SIZE)
                return -EINVAL;

        uint_32 byte_count = blk_size * ZONE_SIZE;
        uint_8 *buf = kmalloc(byte_count);
        if (!buf)
                return -ENOMEM;

        int ret = read_blocks(sb, blk_start, blk_size, buf);
        if (ret < 0) {
                kfree(buf);
                return ret;
        }

        bmap->bits = buf;
        bmap->map_bytes_length = byte_count;
        return 0;
}

int write_bitmap(struct super_block *sb,
                 uint_32 blk_start,
                 uint_32 blk_count,
                 struct bitmap *bmap)
{
        if (!sb || !bmap || !bmap->bits || blk_count == 0 ||
            blk_count > 0xffffffffU / ZONE_SIZE ||
            bmap->map_bytes_length < blk_count * ZONE_SIZE)
                return -EINVAL;

        return write_blocks(sb, blk_start, blk_count, bmap->bits);
}
