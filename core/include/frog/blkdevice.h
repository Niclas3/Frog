#ifndef _BLOCK_BLKDEVICE_H
#define _BLOCK_BLKDEVICE_H

#include <frog/types.h>

#define DISK_NAME_LEN 8

// `struct gendisk` stands for a whole disk
struct gendisk {
        int major;
        int first_minor;
        int minors;

        char name[DISK_NAME_LEN];        // name of disk
        struct ide_channel *my_channel;  // this disk own channel
        uint_8 dev_no;                   // master :0, slave :1
        char * partitions_tlb;           // partition tables
	void *private_data;
};


#endif
