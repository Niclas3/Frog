#include <device/ide.h>
#include <fs/fs.h>
#include <fs/inode.h>

#include <debug.h>
#include <string.h>
#include <sys/memory.h>

#include <fs/super_block.h>
#include <sys/threads.h>
#include <sys/int.h>

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
static void flush_inode(struct partition *part,
                        struct inode *inode,
                        void *io_buf)
{
    // flush buffer must be set
    if (io_buf == NULL) {
        PAINC("Need buffer for flush inode.");
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
    target_inode.i_nlinks = 0;
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
    struct list_head *cur = list;
    while (cur->next != list) {
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
        memcpy(target, buf, sizeof(struct inode));
        target = (struct inode *) (buf + pos.offset);
        target->i_nlinks += 1;

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
void inode_close(struct inode *inode) {
    enum intr_status old_status = intr_disable();
    if(--inode->i_nlinks == 0){
        list_del_init(&inode->inode_tag);
        //free kernel memory
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
void inode_new(uint_32 inode_nr, struct inode *new_inode) {
    new_inode->i_num = inode_nr;
    new_inode->i_size = 0;
    new_inode->i_nlinks = 0;
    new_inode->i_lock = false;
    for(int i=0; i < 12; i++){
        new_inode->i_zones[i] = 0;
    }
}
