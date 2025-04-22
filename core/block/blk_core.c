#include <frog/bio.h>
#include <frog/blk_types.h>
#include <frog/blkdevice.h>
#include <frog/errno.h>
#include <frog/list.h>
#include <frog/memory.h>
#include <frog/string.h>

#define BLOCK_SECTOR 512

void bio_write(struct block_device *hd,
                      unsigned int lba,
                      void *buf,
                      unsigned int sec_cnt)
{
        // make a bio and request add it to request_queue
        hd->bd_disk->fops->request(hd->bd_disk->queue);
}

void bio_read(struct block_device *hd,
                     unsigned int lba,
                     void *buf,
                     unsigned int sec_cnt){
}

/**
 * this for block driver
 * 1. You can use register_blkdev() get a valid major number
 * 2. and alloc a disk , set major number to this disk.major
 * 3. and use blk_init_queue() to initial cmd queue.
 * 4. finally use add_disk() to kernel, it means add struct gendisk to global
 * g_blk_devs[]
 *****************************************************************************/
#define BLKDEV_MAJOR_TABLE_SIZE 255
#define BLKDEV_TABLE_SIZE 255
#define BLKDEV_CUSTOM_NAME 16
static struct blk_dev_major {
        int major;
        char name[BLKDEV_CUSTOM_NAME];
} * g_blk_major_names[BLKDEV_MAJOR_TABLE_SIZE];

static LIST_HEAD(g_blk_devs);

/**
 * register_blkdev
 * alloc a uni-mojor number for block device
 * wirte data to g_blk_major_names table
 * and return that major number
 *
 * @param major if major == 0 then auto-allocate a major number
 * @return return a avaliable major number
 *****************************************************************************/
int register_blkdev(unsigned int major, const char *name)
{
        for (int i = 0; i < BLKDEV_MAJOR_TABLE_SIZE; i++) {
                struct blk_dev_major *major_ele = g_blk_major_names[i];
                if (major_ele->major == 0) {
                        return i;
                }
        }
        return -(ENOSPC);
}

/**
 * unregister_blkdev
 * remove data from g_blk_major_names table by given major number
 * if major is zero it means the end of table.
 *****************************************************************************/
void unregister_blkdev(unsigned int major, const char *name)
{
        for (int i = 0; i < BLKDEV_MAJOR_TABLE_SIZE; i++) {
                struct blk_dev_major *major_ele = g_blk_major_names[i];
                if (!major_ele->major)
                        return;
                if (major_ele->major == major) {
                        memset(major_ele, 0, sizeof(struct blk_dev_major));
                }
        }
}

/**
 * alloc_disk
 *
 *
 * @param part_cnt how many parts this disk has.
 * @return a gendisk object
 *****************************************************************************/
struct gendisk *alloc_disk(int part_cnt)
{
        struct gendisk *disk = sys_malloc(sizeof(struct gendisk));
        INIT_LIST_HEAD(&disk->partitions_list);
        return disk;
}

void free_disk(struct gendisk *disk)
{
        if (disk->private_data) {
                sys_free(disk->private_data);
        }
        if (disk->partitions_tlb) {
                sys_free(disk->partitions_tlb);
        }
        if (disk->queue) {
                sys_free(disk->queue);
        }
        if (disk->fops) {
                sys_free(disk->fops);
        }
}

struct request_queue *blk_init_queue(void(req_fn)(struct request_queue *q),
                                     spinlock_t lock)
{
        struct request_queue *q = sys_malloc(sizeof(struct request_queue));
        return q;
}

/**
 * disk_scan_partitions
 *
 * scan all partitions in this disk add partitions into disk.partitions
 *
 * @param param write here param Comments write here
 * @return return Comments write here
 *****************************************************************************/
static void disk_scan_partitions(struct block_device *bdisk)
{
        // first thing read first sector of this disk
        char *buf = sys_malloc(BLOCK_SECTOR);
        bio_read(bdisk, 0, buf, 1);
        // test the first sector if this disk partition table is MBR or GPT
        // ....
        //



        sys_free(buf);
}

/**
 * add_disk
 * add struct gendisk to global disk table `g_blk_devs`
 *****************************************************************************/
void add_disk(struct gendisk *disk)
{
        // e.g if a disk A has 3 partitions then there are 4 block_device added
        // into g_blk_devs
        // 1. diskA
        // 2. diskApart1
        // 3. diskApart2
        // 4. diskApart3
        // Q: How to know number of block devices this `disk` has?
        // A: You should use disk_scan_partitions() detects `disk` partitions

        struct block_device *diskdev = sys_malloc(sizeof(struct block_device));
        diskdev->bd_dev =
            DEV_NR(disk->major,
                   disk->minors);  // create a block device for target disk
        diskdev->bd_start_lba = 0;
        diskdev->bd_sec_cnt = 1000;  // I don't know how to get number of
        diskdev->bd_disk = disk;
        disk_scan_partitions(diskdev);

        /* list_add_tail(&bdev->bd_target, &g_blk_devs); */
}

void blk_init(void)
{
        // init a global block device table
        INIT_LIST_HEAD(&g_blk_devs);
}
