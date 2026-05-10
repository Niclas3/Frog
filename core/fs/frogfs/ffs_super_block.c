#include <frog/bitmap.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include <frog/math.h>
#include <frog/string.h>
#include "ffs_utils.h"
#include "frogfs.h"
#include "super_block.h"

/**
 * assign a inode at inode bitmap
 *
 * @param part partition which is mounted at fs_init
 * @return return a inode number when success
 *         return -1 when failed
 *****************************************************************************/
int_32 alloc_inode_bitmap(struct super_block *sb)
{
        struct frogfs_super_block *fsb = (struct frogfs_super_block *) sb;
        int_32 idx = find_block_bitmap(fsb->i_bmap, 1);
        if (idx == -1) {
                return -1;
        }
        set_value_bitmap(fsb->i_bmap, idx, 1);
        return idx;
}

int_32 free_inode_bitmap(struct super_block *sb, int_32 index)
{
        ASSERT(index >= 0);
        struct frogfs_super_block *fsb = (struct frogfs_super_block *) sb;
        set_value_bitmap(fsb->i_bmap, index, 0);
        return 0;
}

/**
 * assign a zone at zone bitmap
 *
 * @param frogfs_super_block
 * @return return index
 *****************************************************************************/
int_32 alloc_zone_bitmap(struct super_block *sb)
{
        struct frogfs_super_block *fsb = (struct frogfs_super_block *) sb;
        int_32 idx = find_block_bitmap(fsb->z_bmap, 1);
        if (idx == -1) {
                return -1;
        }
        set_value_bitmap(fsb->z_bmap, idx, 1);
        return idx;
}

int_32 free_znode_bitmap(struct super_block *sb, int_32 index)
{
        ASSERT(index >= 0);
        struct frogfs_super_block *fsb = (struct frogfs_super_block *) sb;
        set_value_bitmap(fsb->z_bmap, index, 0);
        return 0;
}


/**
 * flush bitmaps to disk
 *
 * @param param write here param Comments write here
 * @return return Comments write here
 *****************************************************************************/
void flush_bitmap_block(struct super_block *sb,
                        enum frogfs_bmap_t b_type,
                        int_32 bit_idx)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        uint_32 off_sce = bit_idx / BITS_PER_ZONE;
        // alternative:off_size = bit_idx * 8
        uint_32 off_size = off_sce * ZONE_SIZE;

        uint_32 start_lba;
        uint_8 *bitmap_buf;
        if (b_type == INODE_BITMAP) {
                start_lba = fsb->disk_sb.s_imap_blk + off_sce;
                bitmap_buf = fsb->i_bmap->bits + off_size;
        } else if (b_type == ZONE_BITMAP) {
                start_lba = fsb->disk_sb.s_zmap_blk + off_sce;
                bitmap_buf = fsb->z_bmap->bits + off_size;
        }

        if (write_blocks(sb, start_lba, 1, bitmap_buf) < 0) {
                DEBUG("flush bitmap start: %d, bmap:%x", start_lba, bitmap_buf);
        }
}

int read_bitmap(struct super_block *sb,
                uint_32 blk_start,
                uint_32 blk_size,
                struct bitmap *bmap)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        uint_8 *buf = kmalloc(blk_size * ZONE_SIZE);
        memset(buf, 0, blk_size * ZONE_SIZE);

        int rsize = read_blocks(sb, blk_start, blk_size, buf);
        bmap->bits = buf;
        bmap->map_bytes_length = (blk_size * ZONE_SIZE) / 8;
        return rsize;
}

int write_bitmap(struct super_block *sb,
                 uint_32 blk_start,
                 uint_32 size,
                 struct bitmap *bmap)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        uint_32 blk_size = CEIL(size, ZONE_SIZE);
        int wsize = write_blocks(sb, blk_start, blk_size, bmap->bits);
        return wsize;
}
