#ifndef __DEV_IDE_H__
#define __DEV_IDE_H__
#include <ostype.h>
#include <sys/semaphore.h>
#include <list.h>
#include <bitmap.h>

struct partition {
    uint_32 start_lba;             // start of sector
    uint_32 sec_cnt;               // sector count
    struct disk* my_disk;          // disk of this partition
    struct list_head part_tag;     // list mark
    char name[8];                  // name of this partition
    struct super_block* sb;        // super block of this partition
    struct bitmap block_bitmap;    // bitmap of block
    struct bitmap inode_bitmap;    // bitmap of inode
    struct list_head open_inodes;  // list of open inode 
};

struct disk {
    char name[8];                        // name of disk
    struct ide_channel* my_channel;      // this disk own channel
    uint_8 dev_no;                       // master :0, slave :1
    struct partition prim_partition[4];  // max number of primary partition is 4
    struct partition logic_partition[8]; // only support 8 logic partitions
};

struct ide_channel {
    char name[8];               // name of ata channel
    uint_16 port_base;          // this channel start port
    uint_8 irq_no;              // this channel irq number
    struct lock lock;           // channel lock
    bool expecting_intr;        // if waiting disk interrupt or not
    struct semaphore disk_done; // To block or awake driver
    struct disk devices[2];     // represent master or slave disk
};
void ide_init(void);

#endif
