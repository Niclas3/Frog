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
static int bio_validate_range(struct block_device *bdev,
                              uint_32 lba,
                              uint_32 sec_cnt,
                              const void *buf)
{
        if (!bdev || !bdev->bd_disk || !buf || !sec_cnt)
                return -EINVAL;
        unsigned long long disk_end = bdev->bd_disk->lba_sectors;
        unsigned long long device_start = bdev->bd_start_lba;
        unsigned long long device_end =
            device_start + (unsigned long long) bdev->bd_sec_cnt;
        unsigned long long request_start = lba;
        unsigned long long request_end = request_start + sec_cnt;
        if (!disk_end || !bdev->bd_sec_cnt || device_end < device_start ||
            request_end < request_start || device_end > disk_end ||
            request_start < device_start || request_end > device_end ||
            request_end > disk_end)
                return -EIO;
        return 0;
}

int bio_write(struct block_device *hd,
              unsigned int lba,
              void *buf,
              unsigned int sec_cnt)
{
        int ret = bio_validate_range(hd, lba, sec_cnt, buf);
        if (ret < 0)
                return ret;
        if (hd && hd->bd_disk && hd->bd_disk->bdops &&
            hd->bd_disk->bdops->write) {
                ret = hd->bd_disk->bdops->write(hd, lba, sec_cnt, buf);
                return ret < 0 ? ret : 0;
        } else {
                return -ENODEV;
        }
}

int bio_read(struct block_device *hd,
             unsigned int lba,
             void *buf,
             unsigned int sec_cnt)
{
        int ret = bio_validate_range(hd, lba, sec_cnt, buf);
        if (ret < 0)
                return ret;
        if (hd && hd->bd_disk && hd->bd_disk->bdops &&
            hd->bd_disk->bdops->read) {
                ret = hd->bd_disk->bdops->read(hd, lba, sec_cnt, buf);
                return ret < 0 ? ret : 0;
        } else {
                return -ENODEV;
        }
}


static uint_32 get_available_major(void)
{
        if (!blkdev_bitmap || !blkdev_bitmap->bits)
                return (uint_32) -1;
        uint_32 idx = find_block_bitmap(blkdev_bitmap, 1);
        if (idx == (uint_32) -1 || idx >= BLKDEV_TABLE_SIZE)
                return (uint_32) -1;
        set_value_bitmap(blkdev_bitmap, idx, 1);
        return idx;
}

static uint_32 free_major(int idx){
        if (!blkdev_bitmap || !blkdev_bitmap->bits || idx < 0 ||
            idx >= BLKDEV_TABLE_SIZE)
                return (uint_32) -1;
        set_value_bitmap(blkdev_bitmap, idx, 0);
        return 0;
}

static bool major_is_registered(uint_32 major)
{
        if (!blkdev_bitmap || !blkdev_bitmap->bits ||
            major >= BLKDEV_TABLE_SIZE)
                return false;
        uint_32 value = get_value_bitmap(blkdev_bitmap, major);
        return !!value;
}

const struct block_device_operations *get_blkdev_operations(int major)
{
        if (major < 0 || major >= BLKDEV_TABLE_SIZE)
                return NULL;
        return blkdev_table[major];
}

struct block_device *get_block_device(dev_t dev_no)
{
        struct list_head *pos;
        list_for_each (pos, &g_blk_devs) {
                struct block_device *target =
                    container_of(pos, struct block_device, bd_target);
                if (target && (target->bd_dev == dev_no)) {
                        return target;
                }
        }
        return NULL;
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
        if (!blkdev_bitmap || !blkdev_bitmap->bits || !bdop ||
            major >= BLKDEV_TABLE_SIZE)
                return -1;
        if (major == 0) {
                uint_32 new_major = get_available_major();
                if (new_major == (uint_32) -1) {
                        WARN("[blkdev]: not enough blkdev number.");
                        return -1;
                } else {
                        blkdev_table[new_major] = bdop;
                        return new_major;
                }
        } else {
                if (!major_is_registered(major)) {
                        set_value_bitmap(blkdev_bitmap, major, 1);
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
        if (major >= BLKDEV_TABLE_SIZE) {
                return -1;
        }
        const struct block_device_operations *target = blkdev_table[major];
        if (target) {
                blkdev_table[major] = NULL;
                free_major(major);
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
        if (!disk)
                return NULL;
        memset(disk, 0, sizeof(*disk));
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
        if (!diskdev) {
                unregister_blkdev((uint_32) major);
                return;
        }
        memset(diskdev, 0, sizeof(*diskdev));
        // create a block device for target disk
        diskdev->bd_dev = DEV_NR(disk->major, disk->first_minor);
        diskdev->bd_start_lba = 0;
        diskdev->bd_sec_cnt = disk->lba_sectors;
        diskdev->bd_disk = disk;
        INIT_LIST_HEAD(&diskdev->bd_target);
        INIT_LIST_HEAD(&diskdev->bd_part_node);
        list_add_tail(&diskdev->bd_target, &g_blk_devs);

        disk_scan_partitions(diskdev);
}

struct block_device *alloc_partation_bdev(struct block_device *hd,
                                          uint_32 start_lba,
                                          uint_32 sec_cnt,
                                          int part_index)
{
        struct block_device *bdev = kmalloc(sizeof(*bdev));
        if (!bdev)
                return NULL;
        memset(bdev, 0, sizeof(*bdev));
        bdev->bd_disk = hd->bd_disk;
        bdev->bd_start_lba = start_lba;
        bdev->bd_sec_cnt = sec_cnt;
        bdev->bd_dev = DEV_NR(hd->bd_disk->major, part_index);
        INIT_LIST_HEAD(&bdev->bd_target);
        INIT_LIST_HEAD(&bdev->bd_part_node);
        return bdev;
}


int add_partations_bdev(struct block_device *hd, struct block_device *part_bdev)
{
        if (!hd || !hd->bd_disk || !part_bdev)
                return -EINVAL;
        if (get_block_device(part_bdev->bd_dev))
                return -EEXIST;
        uint_32 major = DEV_MAJOR(part_bdev->bd_dev);
        uint_32 minor = DEV_MINOR(part_bdev->bd_dev);
        char *name = kmalloc(64);
        if (!name)
                return -ENOMEM;
        sprintf(name, "%sp%d", part_bdev->bd_disk->name, minor);
        int ret = devfs_create_node(name, DEV_TYPE_BLOCK, major, minor);
        kfree(name);
        if (ret < 0)
                return ret;
        list_add_tail(&part_bdev->bd_target, &g_blk_devs);
        list_add_tail(&part_bdev->bd_part_node, &hd->bd_disk->partitions_list);
        return 0;
}

int block_init(void)
{
        blkdev_bitmap = kmalloc(sizeof(struct bitmap));
        if (!blkdev_bitmap)
                return -ENOMEM;
        blkdev_bitmap->bits = kmalloc(CEIL(BLKDEV_TABLE_SIZE, 8));
        if (!blkdev_bitmap->bits) {
                kfree(blkdev_bitmap);
                blkdev_bitmap = NULL;
                return -ENOMEM;
        }
        blkdev_bitmap->map_bytes_length = CEIL(BLKDEV_TABLE_SIZE, 8);
        init_bitmap(blkdev_bitmap);

        return 0;
}
