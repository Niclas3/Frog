#include "ffs_utils.h"
#include <frog/math.h>
#include <frog/string.h>

#define BLOCK_NUM_TO_LBA(block_no) ((block_no) << 1)

int read_blocks(struct super_block *sb,
                uint_32 block_start,
                uint_32 count,
                uint_8 *buf)
{
        struct block_device *bdev = sb->s_bdev;
        uint_32 lba_start = BLOCK_NUM_TO_LBA(block_start);
        int size = bio_read(bdev, lba_start, buf, count * SECTOR_PER_ZONE);
        return size;
}

int write_blocks(struct super_block *sb,
                 uint_32 block_start,
                 uint_32 count,
                 uint_8 *buf)
{
        struct block_device *bdev = sb->s_bdev;
        uint_32 lba_start = BLOCK_NUM_TO_LBA(block_start);
        int size = bio_write(bdev, lba_start, buf, count * SECTOR_PER_ZONE);
        return size;
}

static int read_indirect_table(struct super_block *sb,
                               uint_32 start,
                               uint_8 *buf)
{
        int size = read_blocks(sb, start, 1, buf);
        return size;
}


static uint_32 next_blk_num(struct inode *inode, int_32 last_zone_idx)
{
        struct frogfs_inode *finode = (struct frogfs_inode *) inode->i_private;
        struct super_block *sb = inode->i_sb;
        uint_32 indirct_tlb_start;
        uint_32 idx;
        uint_32 *buf = kmalloc(ZONE_SIZE);

        if (last_zone_idx >= FROGFS_FIRST_IND_TLB_NO - 1 &&
            last_zone_idx < FROGFS_SECOND_IND_TLB_NO - 2) {
                indirct_tlb_start = finode->i_zones[11];
                idx = (last_zone_idx - FROGFS_FIRST_IND_TLB_NO) + 1;
                read_indirect_table(sb, indirct_tlb_start, buf);
        } else if (last_zone_idx >= (FROGFS_SECOND_IND_TLB_NO - 1) &&
                   last_zone_idx < FROGFS_THIRD_IND_TLB_NO - 2) {
                indirct_tlb_start = finode->i_zones[12];
                idx = (last_zone_idx - FROGFS_SECOND_IND_TLB_NO) + 1;
                read_indirect_table(sb, indirct_tlb_start, buf);
        } else if (last_zone_idx >= (FROGFS_THIRD_IND_TLB_NO - 1) &&
                   last_zone_idx < FROGFS_FOURTH_IND_TLB_NO - 2) {
                indirct_tlb_start = finode->i_zones[13];
                idx = (last_zone_idx - FROGFS_THIRD_IND_TLB_NO) + 1;
                read_indirect_table(sb, indirct_tlb_start, buf);

        } else if (last_zone_idx >= (FROGFS_FOURTH_IND_TLB_NO - 1) &&
                   last_zone_idx < FROGFS_LAST_IND_TLB_NO - 1) {
                indirct_tlb_start = finode->i_zones[14];
                idx = (last_zone_idx - FROGFS_FOURTH_IND_TLB_NO) + 1;
                read_indirect_table(sb, indirct_tlb_start, buf);
        }
        uint_32 res = buf[idx];
        kfree(buf);
        return res;
}

int_32 next_inode_zone_blk(struct inode *inode, int_32 last_zone_idx)
{
        struct frogfs_inode *finode = inode->i_private;
        struct super_block *sb = inode->i_sb;
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        if (last_zone_idx < 10) {
                return finode->i_zones[last_zone_idx++];
        } else {
                uint_32 next = next_blk_num(inode, last_zone_idx);
                return next;
        }
}


