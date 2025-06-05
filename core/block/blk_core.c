#include <frog/bio.h>
#include <frog/bitmap.h>
#include <frog/blk_types.h>
#include <frog/blkdevice.h>
#include <frog/block.h>
#include <frog/errno.h>
#include <frog/list.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/dev.h>
#include <stdio.h>

#include <frog/compiler.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/panic.h>

#define BLOCK_SECTOR 512
#define BLKDEV_MAJOR_TABLE_SIZE 255
#define BLKDEV_TABLE_SIZE 255
#define BLKDEV_CUSTOM_NAME 16

static const struct block_device_operations *blkdev_table[BLKDEV_TABLE_SIZE];
static struct bitmap *blkdev_bitmap;
static LIST_HEAD(g_blk_devs);  // list of block devices
                               //
extern void msdos_scan_partitions(struct block_device *);

int bio_write(struct block_device *hd,
              unsigned int lba,
              void *buf,
              unsigned int sec_cnt)
{
        if (hd && hd->bd_disk && hd->bd_disk->bdops &&
            hd->bd_disk->bdops->write) {
                return hd->bd_disk->bdops->write(hd, lba, sec_cnt, buf);
        } else {
                return -1;
        }
}

int bio_read(struct block_device *hd,
             unsigned int lba,
             void *buf,
             unsigned int sec_cnt)
{
        if (hd && hd->bd_disk && hd->bd_disk->bdops &&
            hd->bd_disk->bdops->read) {
                return hd->bd_disk->bdops->read(hd, lba, sec_cnt, buf);
        } else {
                return -1;
        }
}


static uint_32 get_avaliable_major()
{
        return find_block_bitmap(blkdev_bitmap, 1);
}

static bool is_avalible_major(uint_32 major)
{
        uint_32 value = get_value_bitmap(blkdev_bitmap, major);
        return !!value;
}

const struct block_device_operations *get_blkdev_bdev(int major)
{
        ASSERT(major < BLKDEV_TABLE_SIZE && major >= 0);
        return blkdev_table[major];
}

/**
 * register_blkdev
 * alloc a uni-mojor number for block device
 * wirte data to g_blk_major_names table
 * and return that major number
 *
 * @param major if major == 0 then auto-allocate a major number
 * @return return a avaliable major number
 *****************************************************************************/

int register_blkdev(unsigned int major, struct block_device_operations *bdop)
{
        if (major == 0) {
                uint_32 new_major = get_avaliable_major();
                if (new_major == -1) {
                        WARN("[blkdev]: not enough blkdev number.");
                        return -1;
                } else {
                        blkdev_table[new_major] = bdop;
                        return new_major;
                }
        } else {
                if (is_avalible_major(major)) {
                        blkdev_table[major] = bdop;
                        return major;
                } else {
                        WARN("[blkdev]: %d is exist", major);
                        return -1;
                }
        }
        return 0;
}

/**
 * unregister_blkdev
 * remove data from g_blk_major_names table by given major number
 * if major is zero it means the end of table.
 *****************************************************************************/
int unregister_blkdev(unsigned int major)
{
        if (major > BLKDEV_TABLE_SIZE) {
                return -1;
        }
        const struct block_device_operations *target = blkdev_table[major];
        if (target) {
                blkdev_table[major] = NULL;
        }
        return 0;
}

/**
 * alloc_disk
 *
 *
 * @param part_cnt how many parts this disk has.
 * @return a gendisk object
 *****************************************************************************/
struct gendisk *alloc_disk()
{
        struct gendisk *disk = kmalloc(sizeof(struct gendisk));
        INIT_LIST_HEAD(&disk->partitions_list);
        return disk;
}

void free_disk(struct gendisk *disk)
{
        if (disk) {
                kfree(disk);
        }
}

static inline bool is_MBR(struct block_device *bdev)
{
        return true;
}

static inline bool is_GPT(struct block_device *bdev)
{
        return false;
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
        ASSERT(bdisk);
        if (likely(is_MBR(bdisk))) {
                msdos_scan_partitions(bdisk);
        } else if (is_GPT(bdisk)) {
                // GPT scan_partitions
        } else {
                PANIC("[block]: unknown partition");
                return;
        }
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

        int major = register_blkdev(0, disk->bdops);
        if (major == -1) {
                PANIC("[block]: not enough block table.");
                return;
        }

        disk->major = major;
        struct block_device *diskdev = kmalloc(sizeof(struct block_device));
        // create a block device for target disk
        diskdev->bd_dev = DEV_NR(disk->major, disk->first_minor);
        diskdev->bd_start_lba = 0;
        diskdev->bd_sec_cnt = disk->lba_sectors;
        diskdev->bd_disk = disk;
        list_add_tail(&diskdev->bd_target, &g_blk_devs);

        disk_scan_partitions(diskdev);
}

struct block_device *alloc_partation_bdev(struct block_device *hd,
                                          uint_32 start_lba,
                                          uint_32 sec_cnt,
                                          int part_index)
{
        struct block_device *bdev = kmalloc(sizeof(*bdev));
        bdev->bd_disk = hd->bd_disk;
        bdev->bd_start_lba = start_lba;
        bdev->bd_sec_cnt = sec_cnt;
        bdev->bd_dev = DEV_NR(hd->bd_disk->major, part_index);
        return bdev;
}


int add_partations_bdev(struct block_device *hd, struct block_device *part_bdev)
{
        list_add_tail(&part_bdev->bd_target, &g_blk_devs);
        list_add_tail(&part_bdev->bd_target, &hd->bd_disk->partitions_list);
        uint_32 major = DEV_MAJOR(part_bdev->bd_dev);
        uint_32 minor = DEV_MINOR(part_bdev->bd_dev);
        char *name = kmalloc(64);
        sprintf(name, "%s%dp%d", part_bdev->bd_disk->name, major, minor);
        devfs_create_node(name, DEV_TYPE_BLOCK, major, minor);
        DEBUG("[partation]: %s %d %d", name, part_bdev->bd_start_lba, part_bdev->bd_sec_cnt);
        kfree(name);
        return 0;
}

void block_init(void)
{
        blkdev_bitmap = kmalloc(sizeof(struct bitmap));
        blkdev_bitmap->bits = kmalloc(CEIL(BLKDEV_TABLE_SIZE, 8));
        blkdev_bitmap->map_bytes_length = CEIL(BLKDEV_TABLE_SIZE, 8);
        init_bitmap(blkdev_bitmap);

        return;
}
