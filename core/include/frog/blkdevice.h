#ifndef _BLOCK_BLKDEVICE_H
#define _BLOCK_BLKDEVICE_H

/* this file is for block driver
 * */
#include <frog/list.h>
#include <frog/spinlock.h>
#include <frog/types.h>

#define DISK_NAME_LEN 8
struct block_device;
struct gendisk;
struct io_scheduler;
struct bio;

typedef enum { BLK_RDONLY, BLK_WRONLY } fmode_t;

struct request_queue {
        struct list_head request_head;      // list of request
        struct gendisk *disk;               // relative disk
        struct io_scheduler *io_scheduler;  // io scheuler for feature
        spinlock_t queue_lock;
        unsigned int queue_depth;  // max requests number of this queue
};


struct request {
        struct request_queue *queue;
        struct list_head request_target;
        struct bio *bio;
        int sector;                 // start lba
        unsigned int nr_sector;     // number of sector
        unsigned int req_cmd_type;  // READ or WRITE
};

struct block_device_operations {
        int (*open)(struct block_device *bdev, fmode_t mode);
        int (*release)(struct gendisk *disk, fmode_t mode);
        int (*ioctl)(struct block_device *bdev,
                     fmode_t mode,
                     unsigned cmd,
                     unsigned args);
        void (*request)(struct request_queue *queue);
        void (*submit_bio)(struct bio *bio);
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
        struct block_device_operations *fops;
        void *private_data;
        struct request_queue *queue;
        struct list_head disk_target;
};

int register_blkdev(unsigned int major, const char *name);
void unregister_blkdev(unsigned int major, const char *name);
struct gendisk *alloc_disk(int part_cnt);
void free_disk(struct gendisk *disk);
struct request_queue *blk_init_queue(void(req_fn)(struct request_queue *q),
                                     spinlock_t lock);
// add disk to global block disk table
void add_disk(struct gendisk *disk);

#endif
