#include <frog/blkdevice.h>
#include <frog/block.h>
#include <frog/errno.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/frogfs_root.h>
#include <kernel/qemu_test.h>
#include <stdio.h>
#include "frogfs.h"
#include "inode.h"
#include "super_block.h"

#define ROOT_TEST_DISK_COUNT 3
#define ROOT_TEST_PARTITION_LBA 2U
#define ROOT_TEST_PARTITION_SECTORS 4096U
#define ROOT_TEST_DISK_SECTORS 8192U

struct root_test_disk {
        struct gendisk *disk;
        struct __frogfs_super_block super;
        int readable;
};

static struct root_test_disk root_test_disks[ROOT_TEST_DISK_COUNT];

static void put_le32(uint_8 *target, uint_32 value)
{
        target[0] = (uint_8) value;
        target[1] = (uint_8) (value >> 8);
        target[2] = (uint_8) (value >> 16);
        target[3] = (uint_8) (value >> 24);
}

static int root_test_read(struct block_device *bdev,
                          uint_32 lba,
                          uint_32 sec_cnt,
                          void *buf)
{
        if (!bdev || !bdev->bd_disk || !buf || sec_cnt != 1)
                return -EIO;
        struct root_test_disk *fixture = bdev->bd_disk->private_data;
        if (!fixture)
                return -EIO;

        memset(buf, 0, 512);
        if (lba == 0) {
                uint_8 *sector = buf;
                sector[446 + 4] = 0x83;
                put_le32(&sector[446 + 8], ROOT_TEST_PARTITION_LBA);
                put_le32(&sector[446 + 12], ROOT_TEST_PARTITION_SECTORS);
                sector[510] = 0x55;
                sector[511] = 0xaa;
                return 0;
        }
        if (lba == ROOT_TEST_PARTITION_LBA && fixture->readable) {
                memcpy(buf, &fixture->super, sizeof(fixture->super));
                return 0;
        }
        return -EIO;
}

static struct block_device_operations root_test_ops = {
    .read = root_test_read,
};

static void root_test_set_super(struct root_test_disk *fixture,
                                const char *label)
{
        struct __frogfs_super_block *disk = &fixture->super;
        memset(disk, 0, sizeof(*disk));
        disk->s_magic = FROGFS_MAGIC;
        strncpy(disk->vol_name, label, sizeof(disk->vol_name));
        disk->s_ninodes = 16;
        disk->s_inode_sz = sizeof(struct frogfs_inode);
        disk->s_nzones = 8;
        disk->s_zone_sz = ZONE_SIZE;
        disk->s_imap_blk = ROOT_TEST_PARTITION_LBA / SECTOR_PER_ZONE + 1;
        disk->s_imap_sz = 1;
        disk->s_zmap_blk = disk->s_imap_blk + disk->s_imap_sz;
        disk->s_zmap_sz = 1;
        disk->s_inode_table_blk = disk->s_zmap_blk + disk->s_zmap_sz;
        disk->s_inode_table_sz =
            DIV_ROUND_UP(disk->s_ninodes * disk->s_inode_sz, ZONE_SIZE);
        disk->s_data_start_blk =
            disk->s_inode_table_blk + disk->s_inode_table_sz;
        disk->root_inode_no = 0;
        disk->dir_entry_size = FROGFS_DIR_ENTRY_SIZE;
        disk->s_log_zone_sz = 1;
        disk->s_max_file_sz = MAX_FILE_SIZE;
        fixture->readable = 1;
}

static int root_test_add_disks(void)
{
        for (uint_32 index = 0; index < ROOT_TEST_DISK_COUNT; index++) {
                struct root_test_disk *fixture = &root_test_disks[index];
                fixture->disk = alloc_disk();
                if (!fixture->disk)
                        return 0;
                fixture->disk->first_minor = 0;
                fixture->disk->minors = 2;
                fixture->disk->bdops = &root_test_ops;
                fixture->disk->private_data = fixture;
                fixture->disk->lba_sectors = ROOT_TEST_DISK_SECTORS;
                sprintf(fixture->disk->name, "rt%d", index);
                root_test_set_super(fixture, "other");
                add_disk(fixture->disk);
        }
        return 1;
}

struct root_test_iteration {
        uint_32 partitions;
        int saw_whole_disk;
};

static int root_test_visit_partition(struct block_device *bdev, void *data)
{
        struct root_test_iteration *iteration = data;
        for (uint_32 index = 0; index < ROOT_TEST_DISK_COUNT; index++) {
                if (bdev->bd_disk != root_test_disks[index].disk)
                        continue;
                iteration->partitions++;
                if (bdev->bd_start_lba == 0)
                        iteration->saw_whole_disk = 1;
        }
        return 0;
}

static int root_test_status(enum frogfs_root_status expected,
                            struct block_device *expected_bdev)
{
        struct frogfs_root_result result = frogfs_locate_root();
        return result.status == expected && result.bdev == expected_bdev;
}

void frogfs_root_locator_regression_run(void)
{
        if (!root_test_add_disks()) {
                frog_test_case("root-locator.fixture", 0);
                return;
        }

        struct root_test_iteration iteration = {0};
        int iter_ret = block_for_each_partition(root_test_visit_partition,
                                                &iteration);
        frog_test_case("root-locator.partitions-only",
                       iter_ret == 0 &&
                           iteration.partitions == ROOT_TEST_DISK_COUNT &&
                           !iteration.saw_whole_disk);

        frog_test_case("root-locator.missing",
                       root_test_status(FROGFS_ROOT_NOT_FOUND, NULL));

        root_test_set_super(&root_test_disks[0], "frog-root");
        struct frogfs_root_result unique = frogfs_locate_root();
        frog_test_case("root-locator.unique",
                       unique.status == FROGFS_ROOT_FOUND && unique.bdev &&
                           unique.bdev->bd_disk == root_test_disks[0].disk);

        root_test_set_super(&root_test_disks[1], "frog-root");
        frog_test_case("root-locator.duplicate",
                       root_test_status(FROGFS_ROOT_DUPLICATE, NULL));

        root_test_set_super(&root_test_disks[0], "other");
        root_test_set_super(&root_test_disks[1], "other");
        root_test_set_super(&root_test_disks[2], "frog-root");
        root_test_disks[2].super.s_magic = 0;
        frog_test_case("root-locator.bad-magic-target",
                       root_test_status(FROGFS_ROOT_CORRUPT, NULL));

        root_test_set_super(&root_test_disks[0], "frog-root");
        root_test_set_super(&root_test_disks[2], "frog-root");
        root_test_disks[2].super.s_zone_sz = 512;
        frog_test_case("root-locator.structural-corrupt-target",
                       root_test_status(FROGFS_ROOT_CORRUPT, NULL));

        root_test_set_super(&root_test_disks[0], "frog-root");
        root_test_set_super(&root_test_disks[1], "other");
        root_test_disks[1].super.s_zone_sz = 512;
        root_test_set_super(&root_test_disks[2], "other");
        frog_test_case("root-locator.ignore-unrelated-corrupt",
                       frogfs_locate_root().status == FROGFS_ROOT_FOUND);

        root_test_disks[2].readable = 0;
        frog_test_case("root-locator.unreadable-is-indeterminate",
                       root_test_status(FROGFS_ROOT_UNREADABLE, NULL));

        root_test_set_super(&root_test_disks[1], "frog-root");
        frog_test_case("root-locator.duplicate-is-definitive",
                       root_test_status(FROGFS_ROOT_DUPLICATE, NULL));
}
