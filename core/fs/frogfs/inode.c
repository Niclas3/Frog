#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/panic.h>

#include <frog/block.h>
#include <frog/irqflags.h>
#include <kernel/debug.h>
#include <kernel/vfs.h>
#include "ffs_utils.h"
#include "frogfs.h"
#include "inode.h"
#include "super_block.h"

struct iposition {
        bool is_crossed;    // cross between 2 sectors
        uint_32 start_blk;  // start sector number
        uint_32 offset;     // offset of this sector
};

/**
 * inode_find()
 * find a inode from disk by inode number
 *
 * @param inode_rn inode number
 * @param *ipos inode position struct return into this
 *****************************************************************************/
static void locale_inode(struct super_block *sb,
                         uint_32 inode_nr,
                         struct iposition *ipos)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        ASSERT(inode_nr < fsb->disk_sb.s_ninodes);
        uint_32 inode_table = fsb->disk_sb.s_inode_table_blk;
        uint_32 inode_offset =
            inode_nr * sizeof(struct frogfs_inode);  // in bytes
        uint_32 inode_blk_offset = inode_offset / ZONE_SIZE;
        uint_32 inode_in_blk_offset = inode_offset % ZONE_SIZE;
        ipos->start_blk = inode_table + inode_blk_offset;

        ipos->is_crossed =
            (ZONE_SIZE - inode_in_blk_offset) < sizeof(struct inode) ? true
                                                                     : false;
        ipos->offset = inode_in_blk_offset;
}


/**
 * inode_open
 * open a inode (aka load inode from disk to memory and add it to open_inode at
 * partition)
 *
 * @param part target partition
 * @param inode_nr inode number
 *
 * @return a inode
 *****************************************************************************/
static struct inode *find_open_inode(struct list_head *list, uint_32 inode_nr)
{
        struct inode *target;
        struct list_head *cur = list->next;
        while (cur != list) {
                target = container_of(cur, struct inode, i_active_node);
                if (target->i_num == inode_nr) {
                        return target;
                }
                cur = cur->next;
        }
        return NULL;
}


struct inode *geti(struct super_block *sb, uint_32 inode_nr)
{
        // 0.Test part is mounted partition
        ASSERT(sb);
        if (!sb) {
                PANIC("Not a mounted partition.");
        }

        // 1. lookup open_inode at partition if there is a inode number match
        //    inode_nr return it.

        struct inode *target = find_open_inode(&sb->s_inodes, inode_nr);
        if (!target) {
                // 2. if there is not any inode number at open_inode,
                // inode_find() at disk
                //    and add it to open_inode
                struct iposition pos = {0};
                locale_inode(sb, inode_nr, &pos);
                uint_32 read_sz = pos.is_crossed ? 2 : 1;
                target = kmalloc(sizeof(struct inode));
                struct frogfs_inode *finode =
                    kmalloc(sizeof(struct frogfs_inode));

                uint_8 *buf = kmalloc(read_sz * ZONE_SIZE);

                if (read_blocks(sb, pos.start_blk, read_sz, buf) < 0) {
                        DEBUG("not read any block");
                        return NULL;
                }
                memcpy(finode, buf + pos.offset, sizeof(struct frogfs_inode));
                target->i_private = finode;
                target->i_count += 1;

                list_add(&target->i_active_node, &sb->s_inodes);
                sys_free(buf);
        }
        return target;
}

/**
 * inode_new
 *
 * new a inode with inode number
 *
 * @param inode_nr inode number
 * @param new_inode returned inode
 * @return void
 *****************************************************************************/
void new_inode(uint_32 inode_nr, struct inode *new_inode)
{
        new_inode->i_num = inode_nr;
        new_inode->i_size = 0;
        new_inode->i_count = 0;
        new_inode->i_lock = false;
        struct frogfs_inode *finode =
            (struct frogfs_inode *) new_inode->i_private;
        for (int i = 0; i < ZONE_IDX_MAX; i++) {
                finode->i_zones[i] = 0;
        }
}

/**
 * delete inode at inode table
 *
 * help function
 *
 *****************************************************************************/
static void inode_delete(struct super_block *sb, uint_32 inode_no, void *buf)
{
        // Recycle inode_bitmap
        struct iposition inode_pos = {0};
        locale_inode(sb, inode_no, &inode_pos);
        char *inode_buf = buf;
        struct block_device *bdev = sb->s_bdev;

        if (inode_pos.is_crossed) {
                read_blocks(sb, inode_pos.start_blk, 2, buf);
                memset(inode_buf + inode_pos.offset, 0, sizeof(struct inode));
                write_blocks(sb, inode_pos.start_blk, 2, buf);
        } else {
                read_blocks(sb, inode_pos.start_blk, 1, buf);
                memset(inode_buf + inode_pos.offset, 0, sizeof(struct inode));
                write_blocks(sb, inode_pos.start_blk, 1, buf);
        }
}


/*
 * Flat in-direct block and double in-direct block to all_zones[]
 *
 * total 13 i_zones
 *
 * No.0 ~ No.10   i_zones is direct zone address
 *
 * No.11          i_zones is indirect zone address
 * No.12          i_zones is indirect zone address
 * No.13          i_zones is indirect zone address
 * No.14          i_zones is indirect zone address
 *
 * Load (0 ~ max_zone_idx] blocks from inode, include max_zone_idx
 *
 * */
static void resolve_inode_all_zones(struct super_block *sb,
                                    struct inode *inode,
                                    uint_32 *zone_buf,
                                    uint_32 max_zone_idx)
{
        struct frogfs_inode *finode = (struct frogfs_inode *) inode->i_private;
        if (max_zone_idx < 11) {
                for (int i = 0; i < max_zone_idx; i++) {
                        zone_buf[i] = finode->i_zones[i];
                }
                return;
        } else if (max_zone_idx >= 11 && max_zone_idx < 11 + (ZONE_SIZE / 4)) {
                int i = 0;
                int j;
                uint_32 size;
                int current = max_zone_idx;
                for (i = 0; i < max_zone_idx && i < 11; i++) {
                        zone_buf[i] = finode->i_zones[i];
                        current--;
                }
                uint_8 *indirect_zones = kmalloc(ZONE_SIZE);
                if (!indirect_zones) {
                        PANIC("[frogfs]: inode not enough memory.");
                }
                size = read_blocks(sb, finode->i_zones[11], 1, indirect_zones);
                if (size <= 0) {
                        DEBUG("Maybe broken inode.");
                        return;
                }
                for (j = 0; (j + i) < max_zone_idx && j < (ZONE_SIZE / 4);
                     j++) {
                        zone_buf[i + j] = indirect_zones[j];
                        current--;
                }
                if (current <= 0) {
                        return;
                }

                memset(indirect_zones, 0, ZONE_SIZE);
                size = read_blocks(sb, finode->i_zones[12], 1, indirect_zones);
                if (size <= 0) {
                        DEBUG("Maybe broken inode.");
                        return;
                }

                for (int k = 0;
                     (j + i + k) < max_zone_idx && k < (ZONE_SIZE / 4); k++) {
                        zone_buf[i + j + k] = indirect_zones[k];
                }
                kfree(indirect_zones);
                return;
        }
}

/**
 * inode_flush
 * flush inode from memory to disk
 *
 * @param part
 * @param inode inode that be flushed
 * @param io_buf io buffer for flushing need 2 block size
 *
 * @return return Comments write here
 *****************************************************************************/
void flush_inode(struct super_block *sb, struct inode *inode, void *io_buf)
{
        // flush buffer must be set
        if (io_buf == NULL) {
                PANIC("Need buffer for flush inode.");
        }

        struct iposition pos = {0};
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        struct block_device *bdev = sb->s_bdev;

        uint_8 *buf = (uint_8 *) io_buf;
        locale_inode(sb, inode->i_num, &pos);
        ASSERT(pos.start_blk <= (fsb->disk_sb.s_inode_table_blk +
                                 fsb->disk_sb.s_inode_table_sz));
        struct frogfs_inode *target_inode = inode->i_private;

        target_inode->i_gid = inode->i_gid;
        target_inode->i_uid = inode->i_uid;
        target_inode->i_atime = inode->i_atime;
        target_inode->i_ctime = inode->i_ctime;
        target_inode->i_mtime = inode->i_mtime;
        target_inode->i_mode = inode->i_mode;
        target_inode->i_size = inode->i_size;

        if (pos.is_crossed) {
                read_blocks(sb, pos.start_blk, 2, buf);
                memcpy(&buf[pos.offset], &target_inode,
                       sizeof(struct frogfs_inode));
                write_blocks(sb, pos.start_blk, 2, buf);
        } else {
                read_blocks(sb, pos.start_blk, 1, buf);
                memcpy(&buf[pos.offset], &target_inode,
                       sizeof(struct frogfs_inode));
                write_blocks(sb, pos.start_blk, 1, buf);
        }
}


static int free_inode_zones(struct inode *inode)
{
        ASSERT(inode && inode->i_sb);
        struct frogfs_inode *finode = inode->i_private;
        struct super_block *sb = inode->i_sb;
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        struct bitmap *zone_bmap = fsb->z_bmap;
        int_32 zone_idx = 0;
        int_32 zone_bitmap_blk = finode->i_zones[zone_idx];
        while (zone_bitmap_blk) {
                free_znode_bitmap(sb, zone_idx);
                zone_bitmap_blk = next_inode_zone_blk(inode, zone_idx);
                zone_idx = zone_bitmap_blk - fsb->disk_sb.s_data_start_blk;
        }
        return 0;
}

static int free_inode_number(struct super_block *sb, int inode_num)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        set_value_bitmap(fsb->i_bmap, inode_num, 0);
        return 0;
}

/** When we delete inode, we should recycle some resources */
/** 1. inode bitmap of this inode */
/** 2. inode table bit of this inode */
/** 3. inode zones include i_zones[0~11] and indirect table *i_zones[12] */
/** 4. zone bitmap */
void release_inode(struct super_block *sb, uint_32 inode_nr)
{
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;
        struct inode *inode_need_del = geti(sb, inode_nr);

        if (free_inode_zones(inode_need_del) < 0) {
                DEBUG("free_inode_zones error");
                return;
        }

        free_inode_number(sb, inode_nr);

#ifdef DEBUG
        void *io_buf = sys_malloc(2 * ZONE_SIZE);
        inode_delete(sb, inode_nr, io_buf);
        sys_free(io_buf);
#endif
}
