#include "ffs_utils.h"
#include <frog/errno.h>
#include <frog/irqflags.h>
#include <frog/math.h>
#include <frog/string.h>

#ifdef CONFIG_QEMU_TEST
struct frogfs_test_io_failpoint {
        uint_32 fail_after;
        uint_32 failures_left;
        uint_32 matching_calls;
        uint_32 mask;
};

static struct frogfs_test_io_failpoint frogfs_io_failpoint;

void frogfs_test_fail_io_after(uint_32 fail_after,
                               uint_32 failures,
                               uint_32 mask)
{
        unsigned long flags;
        local_irq_save(flags);
        memset(&frogfs_io_failpoint, 0, sizeof(frogfs_io_failpoint));
        if (fail_after && failures &&
            (mask & (FROGFS_TEST_IO_READ | FROGFS_TEST_IO_WRITE))) {
                frogfs_io_failpoint.fail_after = fail_after;
                frogfs_io_failpoint.failures_left = failures;
                frogfs_io_failpoint.mask = mask;
        }
        local_irq_restore(flags);
}

void frogfs_test_clear_io_failpoint(void)
{
        unsigned long flags;
        local_irq_save(flags);
        memset(&frogfs_io_failpoint, 0, sizeof(frogfs_io_failpoint));
        local_irq_restore(flags);
}

static bool frogfs_test_should_fail_io(uint_32 operation, bool metadata)
{
        bool fail = false;
        unsigned long flags;
        local_irq_save(flags);
        uint_32 class_mask = FROGFS_TEST_IO_METADATA | FROGFS_TEST_IO_DATA;
        bool class_matches = !(frogfs_io_failpoint.mask & class_mask) ||
                             (metadata &&
                              (frogfs_io_failpoint.mask &
                               FROGFS_TEST_IO_METADATA)) ||
                             (!metadata &&
                              (frogfs_io_failpoint.mask &
                               FROGFS_TEST_IO_DATA));
        if (frogfs_io_failpoint.fail_after &&
            (frogfs_io_failpoint.mask & operation) && class_matches) {
                frogfs_io_failpoint.matching_calls++;
                if (frogfs_io_failpoint.matching_calls >=
                        frogfs_io_failpoint.fail_after &&
                    frogfs_io_failpoint.failures_left) {
                        frogfs_io_failpoint.failures_left--;
                        fail = true;
                        if (!frogfs_io_failpoint.failures_left)
                                frogfs_io_failpoint.fail_after = 0;
                }
        }
        local_irq_restore(flags);
        return fail;
}
#endif

static int frogfs_check_lba_range(struct block_device *bdev,
                                  unsigned long long lba_start,
                                  unsigned long long sector_count)
{
        if (!bdev || !bdev->bd_disk || !bdev->bd_disk->bdops ||
            !sector_count || !bdev->bd_sec_cnt ||
            !bdev->bd_disk->lba_sectors)
                return -ENODEV;

        unsigned long long partition_start = bdev->bd_start_lba;
        unsigned long long partition_end =
            partition_start + (unsigned long long) bdev->bd_sec_cnt;
        unsigned long long disk_end = bdev->bd_disk->lba_sectors;
        unsigned long long lba_end = lba_start + sector_count;

        if (partition_end < partition_start || partition_end > disk_end ||
            lba_end < lba_start || lba_start < partition_start ||
            lba_end > partition_end || lba_end > disk_end ||
            lba_start > 0xffffffffULL || sector_count > 0xffffffffULL)
                return -EIO;
        return 0;
}

int frogfs_validate_bdev(struct block_device *bdev)
{
        return frogfs_check_lba_range(bdev, bdev ? bdev->bd_start_lba : 0,
                                      bdev ? bdev->bd_sec_cnt : 0);
}

int frogfs_read_super_sector(struct block_device *bdev,
                             struct __frogfs_super_block *disk_sb)
{
        if (!disk_sb)
                return -EINVAL;
        int ret = frogfs_check_lba_range(bdev, bdev ? bdev->bd_start_lba : 0,
                                         1);
        if (ret < 0)
                return ret;
#ifdef CONFIG_QEMU_TEST
        if (frogfs_test_should_fail_io(FROGFS_TEST_IO_READ, true))
                return -EIO;
#endif
        ret = bio_read(bdev, bdev->bd_start_lba, disk_sb, 1);
        return ret < 0 ? -EIO : 0;
}

int frogfs_write_super_sector(struct block_device *bdev,
                              const struct __frogfs_super_block *disk_sb)
{
        if (!disk_sb)
                return -EINVAL;
        int ret = frogfs_check_lba_range(bdev, bdev ? bdev->bd_start_lba : 0,
                                         1);
        if (ret < 0)
                return ret;
#ifdef CONFIG_QEMU_TEST
        if (frogfs_test_should_fail_io(FROGFS_TEST_IO_WRITE, true))
                return -EIO;
#endif
        ret = bio_write(bdev, bdev->bd_start_lba, (void *) disk_sb, 1);
        return ret < 0 ? -EIO : 0;
}

void frogfs_mark_needs_fsck(struct super_block *sb)
{
        if (!sb || !sb->s_fs_info)
                return;
        struct frogfs_super_block *fsb = sb->s_fs_info;
        fsb->needs_fsck = true;
        fsb->disk_sb.s_rd_only = 1;
        if (frogfs_check_lba_range(sb->s_bdev, sb->s_bdev->bd_start_lba,
                                   1) == 0)
                bio_write(sb->s_bdev, sb->s_bdev->bd_start_lba,
                          &fsb->disk_sb, 1);
}

static int frogfs_check_io_range(struct super_block *sb,
                                 uint_32 block_start,
                                 uint_32 count)
{
        if (!sb || !sb->s_bdev || count == 0)
                return -EINVAL;

        struct block_device *bdev = sb->s_bdev;
        unsigned long long lba_start =
            (unsigned long long) block_start * SECTOR_PER_ZONE;
        unsigned long long sector_count =
            (unsigned long long) count * SECTOR_PER_ZONE;
        return frogfs_check_lba_range(bdev, lba_start, sector_count);
}

int read_blocks(struct super_block *sb,
                uint_32 block_start,
                uint_32 count,
                uint_8 *buf)
{
        if (!buf)
                return -EINVAL;
        int ret = frogfs_check_io_range(sb, block_start, count);
        if (ret < 0)
                return ret;

        struct block_device *bdev = sb->s_bdev;
#ifdef CONFIG_QEMU_TEST
        struct frogfs_super_block *fsb = sb->s_fs_info;
        bool metadata = fsb && block_start < fsb->disk_sb.s_data_start_blk;
        if (frogfs_test_should_fail_io(FROGFS_TEST_IO_READ, metadata))
                return -EIO;
#endif
        uint_32 lba_start = block_start * SECTOR_PER_ZONE;
        ret = bio_read(bdev, lba_start, buf, count * SECTOR_PER_ZONE);
        return ret < 0 ? -EIO : 0;
}

int write_blocks(struct super_block *sb,
                 uint_32 block_start,
                 uint_32 count,
                 const uint_8 *buf)
{
        if (!buf)
                return -EINVAL;
        int ret = frogfs_check_io_range(sb, block_start, count);
        if (ret < 0)
                return ret;

        struct block_device *bdev = sb->s_bdev;
#ifdef CONFIG_QEMU_TEST
        struct frogfs_super_block *fsb = sb->s_fs_info;
        bool metadata = fsb && block_start < fsb->disk_sb.s_data_start_blk;
        if (frogfs_test_should_fail_io(FROGFS_TEST_IO_WRITE, metadata))
                return -EIO;
#endif
        uint_32 lba_start = block_start * SECTOR_PER_ZONE;
        ret = bio_write(bdev, lba_start, (void *) buf,
                        count * SECTOR_PER_ZONE);
        return ret < 0 ? -EIO : 0;
}
