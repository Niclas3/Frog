#include <device/ide.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>

#include <debug.h>
#include <string.h>
#include <sys/memory.h>

#include <fs/super_block.h>
#include <sys/int.h>
#include <sys/threads.h>

#define HAS_INODE(inode, nr) ((inode).i_num == (nr))

struct iposition {
    bool is_crossed;    // cross between 2 sectors
    uint_32 start_lba;  // start sector number
    uint_32 offset;     // offset of this sector
};

/**
 * inode_find()
 * find a inode from disk by inode number
 *
 * @param inode_rn inode number
 * @param *ipos inode position struct return into this
 *****************************************************************************/
static void locale_inode(struct partition *part,
                         uint_32 inode_nr,
                         struct iposition *ipos)
{
    ASSERT(inode_nr < part->sb->s_ninodes);
    uint_32 inode_table = part->sb->s_inode_table_lba;
    uint_32 inode_offset = inode_nr * sizeof(struct inode);  // in bytes
    uint_32 inode_sect_offset = inode_offset / SECTOR_SIZE;
    uint_32 inode_in_sect_offset = inode_offset % SECTOR_SIZE;
    ipos->start_lba = inode_table + inode_sect_offset;

    // test if this inode cross 2 sectors
    ipos->is_crossed =
        (SECTOR_SIZE - inode_in_sect_offset) < sizeof(struct inode) ? true
                                                                    : false;
    ipos->offset = inode_in_sect_offset;
}

/**
 * inode_flush
 * flush inode from memory to disk
 *
 * @param part
 * @param inode inode that be flushed
 * @param io_buf io buffer for flushing
 *
 * @return return Comments write here
 *****************************************************************************/
void flush_inode(struct partition *part, struct inode *inode, void *io_buf)
{
    // flush buffer must be set
    if (io_buf == NULL) {
        PANIC("Need buffer for flush inode.");
    }
    struct iposition pos = {0};
    struct disk *hd = part->my_disk;
    uint_8 *buf = (uint_8 *) io_buf;
    locale_inode(part, inode->i_num, &pos);
    ASSERT(pos.start_lba <=
           (part->sb->s_inode_table_lba + part->sb->s_inode_table_sz));
    struct inode target_inode = {0};
    memcpy(&target_inode, inode, sizeof(struct inode));
    // remove those when flush into disk
    target_inode.i_count = 0;
    target_inode.i_lock = false;
    target_inode.inode_tag.prev = target_inode.inode_tag.next = NULL;

    if (pos.is_crossed) {
        ide_read(hd, pos.start_lba, buf, 2);
        memcpy(&buf[pos.offset], &target_inode, sizeof(struct inode));
        ide_write(hd, pos.start_lba, buf, 2);
    } else {
        ide_read(hd, pos.start_lba, buf, 1);
        memcpy(&buf[pos.offset], &target_inode, sizeof(struct inode));
        ide_write(hd, pos.start_lba, buf, 1);
    }
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
        target = container_of(cur, struct inode, inode_tag);
        if (target->i_num == inode_nr) {
            return target;
        }
        cur = cur->next;
    }
    return NULL;
}
struct inode *inode_open(struct partition *part, uint_32 inode_nr)
{
    // 0.Test part is mounted partition
    ASSERT(part->sb);
    if (!part->sb) {
        PANIC("Not a mounted partition.");
    }

    // 1. lookup open_inode at partition if there is a inode number match
    //    inode_nr return it.
    struct inode *target = find_open_inode(&part->open_inodes, inode_nr);
    if (!target) {
        // 2. if there is not any inode number at open_inode, inode_find() at
        // disk
        //    and add it to open_inode
        struct iposition pos = {};
        locale_inode(part, inode_nr, &pos);
        uint_32 read_sz = pos.is_crossed ? 2 : 1;

        // TODO: Maybe can add sys_kmalloc() at memory.c for allocating kernel
        //       memory
        TCB_t *cur = running_thread();
        uint_32 *cur_pagedir_bak = cur->pgdir;
        cur->pgdir = NULL;
        target = sys_malloc(
            sizeof(struct inode));  // this memory at kernel for share
        cur->pgdir = cur_pagedir_bak;
        //--------------------------------------------------------------------

        uint_8 *buf = sys_malloc(read_sz * SECTOR_SIZE);
        ide_read(part->my_disk, pos.start_lba, buf, read_sz);
        memcpy(target, buf + pos.offset, sizeof(struct inode));
        target->i_count += 1;

        list_add(&target->inode_tag, &part->open_inodes);
        sys_free(buf);
    }
    return target;
}

/**
 * inode_close
 *
 * close inode aka remove this inode from open_inode list
 *
 * @param part target partition
 * @param inode_nr inode number
 * @return void
 *****************************************************************************/
void inode_close(struct inode *inode)
{
    enum intr_status old_status = intr_disable();
    if (--inode->i_count == 0) {
        list_del_init(&inode->inode_tag);
        // free kernel memory
        TCB_t *cur = running_thread();
        uint_32 *cur_pagedir_bak = cur->pgdir;
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagedir_bak;
    }
    intr_set_status(old_status);
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
    for (int i = 0; i < 13; i++) {
        new_inode->i_zones[i] = 0;
    }
}

/**
 * delete inode at inode table
 *
 * help function
 *
 *****************************************************************************/
static void inode_delete(struct partition *part, uint_32 inode_no, void *io_buf)
{
    // Recycle inode_bitmap
    struct iposition inode_pos;
    locale_inode(part, inode_no, &inode_pos);
    char *inode_buf = io_buf;

    if (inode_pos.is_crossed) {
        ide_read(part->my_disk, inode_pos.start_lba, inode_buf, 2);
        memset(inode_buf + inode_pos.offset, 0, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.start_lba, inode_buf, 2);
    } else {
        ide_read(part->my_disk, inode_pos.start_lba, inode_buf, 1);
        memset(inode_buf + inode_pos.offset, 0, sizeof(struct inode));
        ide_write(part->my_disk, inode_pos.start_lba, inode_buf, 1);
    }
}

static void inode_all_zones(struct partition *part,
                     struct inode *inode,
                     uint_32 *all_zones)
{
    // Find first accessible i_zones[i]
    // 1. read all i_zones;
    // First 12 i_zones[] elements is direct address of data (aka dir_entry)
    for (int i = 0; i < 12 && inode->i_zones[i]; i++) {
        all_zones[i] = inode->i_zones[i];
    }
    // the 13th i_zones[] is a in-direct table which size is ZONE_SIZE bytes
    if (inode->i_zones[12]) {
        ide_read(part->my_disk, inode->i_zones[12], all_zones + 12, 1);
    }
}

/*  * When we delete inode, we should recycle some resources */
/*  * 1. inode bitmap of this inode */
/*  * 2. inode table bit of this inode */
/*  * 3. inode zones include i_zones[0~11] and indirect table *i_zones[12] */
/*  * 4. zone bitmap */
void inode_release(struct partition *part, uint_32 inode_nr)
{
    struct inode *inode_need_del = inode_open(part, inode_nr);
    uint_32 zone_bitmap_idx;
    // Find first accessible i_zones[i]
    // 1. read all i_zones;
    uint_32 all_zones[MAX_ZONE_COUNT + 1] = {0};
    inode_all_zones(part, inode_need_del, all_zones);
    // Start recycle
    // Recycle zones
    for (uint_32 zone_idx = 0;
         all_zones[zone_idx] && zone_idx < MAX_ZONE_COUNT + 1; zone_idx++) {
        zone_bitmap_idx = 0;
        zone_bitmap_idx = all_zones[zone_idx] - part->sb->s_data_start_lba;
        ASSERT(zone_bitmap_idx > 0);
        set_value_bitmap(&part->zone_bitmap, zone_bitmap_idx, 0);
        flush_bitmap(part, ZONE_BITMAP, zone_bitmap_idx);
    };
    // Recycle i_zones[12]
    if (inode_need_del->i_zones[12]) {
        zone_bitmap_idx =
            inode_need_del->i_zones[12] - part->sb->s_data_start_lba;
        ASSERT(zone_bitmap_idx > 0);
        set_value_bitmap(&part->zone_bitmap, zone_bitmap_idx, 0);
        flush_bitmap(part, ZONE_BITMAP, zone_bitmap_idx);
    }

    // recycle inode space
    set_value_bitmap(&part->inode_bitmap, inode_nr, 0);
    flush_bitmap(part, INODE_BITMAP, zone_bitmap_idx);

    // delete inode  for test-----------------
    void *io_buf = sys_malloc(1024);
    inode_delete(part, inode_nr, io_buf);
    sys_free(io_buf);
    //-------------------------------
    inode_close(inode_need_del);
}
