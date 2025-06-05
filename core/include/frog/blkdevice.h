#ifndef _BLOCK_BLKDEVICE_H
#define _BLOCK_BLKDEVICE_H

/* this file is for block driver
 * */
#include <frog/list.h>
#include <frog/spinlock.h>
#include <frog/types.h>

#define DISK_NAME_LEN 16
struct block_device;
struct gendisk;

typedef enum { BLK_RDONLY, BLK_WRONLY } fmode_t;


struct block_device_operations {
        int (*open)(struct block_device *bdev, fmode_t mode);
        int (*release)(struct gendisk *disk, fmode_t mode);
        int (*ioctl)(struct block_device *bdev,
                     fmode_t mode,
                     unsigned cmd,
                     unsigned args);
        int (*read)(struct block_device *bdev,
                    uint_32 lba,
                    uint_32 sec_cnt,
                    void *buf);
        int (*write)(struct block_device *bdev,
                     uint_32 lba,
                     uint_32 sec_cnt,
                     void *buf);
};

struct block_partitions {
        int start_lba;
        int sector_cnt;
        struct gendisk *disk;
        struct list_head bpartition_target;
};

// `struct gendisk` stands for a whole disk
struct gendisk {
        int major;                         // major number of device
        int first_minor;                   // first minor device number
        int minors;                        // max number of sub-device
        char name[DISK_NAME_LEN];          // name of disk
        struct list_head partitions_list;  // partition tables
        struct block_device_operations *bdops;
        void *private_data;
        unsigned int lba_sectors;  // all sectors number
        struct list_head disk_target;
};

int register_blkdev(unsigned int major, struct block_device_operations *bdop);
int unregister_blkdev(unsigned int major);
const struct block_device_operations *get_blkdev_bdev(int major);

struct block_device *alloc_partation_bdev(struct block_device *hd,
                                          uint_32 start_lba,
                                          uint_32 sec_cnt,
                                          int part_index);
int add_partations_bdev(struct block_device *hd,
                        struct block_device *part_bdev);

struct gendisk *alloc_disk(void);
void free_disk(struct gendisk *disk);
// add disk to global block disk table
void add_disk(struct gendisk *disk);

#endif
