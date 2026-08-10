#ifndef __KERNEL_SYSTEM_ROOT_H
#define __KERNEL_SYSTEM_ROOT_H

#include <kernel/frogfs_root.h>

struct block_device;
struct super_block;

enum frogfs_root_activation_status {
        FROGFS_ROOT_ACTIVATED = 0,
        FROGFS_ROOT_ACTIVATE_BAD_RESULT,
        FROGFS_ROOT_ACTIVATE_REGISTER_FAILED,
        FROGFS_ROOT_ACTIVATE_MKDIR_FAILED,
        FROGFS_ROOT_ACTIVATE_MOUNT_FAILED,
        FROGFS_ROOT_ACTIVATE_SOURCE_MISMATCH,
        FROGFS_ROOT_ACTIVATE_NOT_READ_ONLY,
        FROGFS_ROOT_ACTIVATE_SWITCH_FAILED,
};

struct frogfs_root_activation {
        enum frogfs_root_activation_status status;
        struct block_device *bdev;
        struct super_block *sb;
        int error;
        /* Cleanup failure after a pre-mount failure; error stays primary. */
        int cleanup_error;
};

/* The block registry retains ownership of located->bdev. */
struct frogfs_root_activation frogfs_activate_root(
    const struct frogfs_root_result *located);

#ifdef CONFIG_FROG_TEST_ROOT_NAMESPACE
void frogfs_root_namespace_regression_run(void);
#endif

#endif
