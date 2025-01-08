#ifndef _BLOCK_BLK_TYPES_H
#define _BLOCK_BLK_TYPES_H
#include <frog/types.h>
#include <frog/blkdevice.h>

// struct block_device is a abstruct for a partition
// block subsystem only care about partitions.
// so identify a block_device only need a lba start and sector count 
struct block_device {
        uint_32                 bd_start_lba;             // start of sector
        uint_32                 bd_sec_cnt;               // all sector count of this partition
        struct disk *           bd_disk;          // disk of this partition
};

#endif
