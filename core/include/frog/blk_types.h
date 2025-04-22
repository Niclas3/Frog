#ifndef _BLOCK_BLK_TYPES_H
#define _BLOCK_BLK_TYPES_H
#include <frog/blkdevice.h>
#include <frog/list.h>
#include <frog/types.h>


// struct block_device is a abstruct for a partition
// block subsystem only care about partitions.
// so identify a block_device only need a lba start and sector count
struct block_device {
        uint_32 bd_start_lba;  // start of sector
        uint_32 bd_sec_cnt;    // all sector count of this partition
        struct list_head bd_target;
        dev_t bd_dev;             // contain major and minor number
        struct gendisk *bd_disk;  // disk of this partition
};

// first 16bits is major number
#define DEV_MAJOR(dev_nr) ((dev_nr) & (0xffff << 16)) >> 16
// second 16bits is minor number
#define DEV_MINOR(dev_nr) (dev_nr) & 0xffff
#define DEV_NR(major, minor) (major << 16) | (minor & 0xffff)

#endif
