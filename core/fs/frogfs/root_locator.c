#include <frog/block.h>
#include <frog/string.h>
#include <kernel/frogfs_root.h>
#include "ffs_utils.h"

#define FROGFS_ROOT_VOLUME "frog-root"

struct frogfs_root_scan {
        struct block_device *match;
        uint_32 matches;
        int target_corrupt;
        int unreadable;
};

static bool frogfs_volume_name_is(
    const struct __frogfs_super_block *disk,
    const char *expected)
{
        uint_32 length = strlen(expected);
        return length < sizeof(disk->vol_name) &&
               !memcmp(disk->vol_name, expected, length) &&
               disk->vol_name[length] == '\0';
}

static int frogfs_root_probe_partition(struct block_device *bdev, void *data)
{
        struct frogfs_root_scan *scan = data;
        struct __frogfs_super_block disk;
        enum frogfs_probe_status status =
            frogfs_probe_superblock(bdev, &disk);

        if (status == FROGFS_PROBE_UNREADABLE) {
                scan->unreadable = 1;
                return 0;
        }

        bool target = frogfs_volume_name_is(&disk, FROGFS_ROOT_VOLUME);
        if (status == FROGFS_PROBE_NOT_FROGFS ||
            status == FROGFS_PROBE_CORRUPT) {
                if (target)
                        scan->target_corrupt = 1;
                return 0;
        }
        if (!target)
                return 0;

        if (!scan->matches)
                scan->match = bdev;
        scan->matches++;
        return 0;
}

struct frogfs_root_result frogfs_locate_root(void)
{
        struct frogfs_root_scan scan = {0};
        struct frogfs_root_result result = {
            .status = FROGFS_ROOT_NOT_FOUND,
            .bdev = NULL,
        };

        int ret = block_for_each_partition(frogfs_root_probe_partition,
                                           &scan);
        if (scan.matches >= 2) {
                result.status = FROGFS_ROOT_DUPLICATE;
        } else if (scan.target_corrupt) {
                result.status = FROGFS_ROOT_CORRUPT;
        } else if (ret < 0 || scan.unreadable) {
                result.status = FROGFS_ROOT_UNREADABLE;
        } else if (scan.matches == 1) {
                result.status = FROGFS_ROOT_FOUND;
                result.bdev = scan.match;
        }
        return result;
}
