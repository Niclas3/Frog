#ifndef __KERNEL_FROGFS_ROOT_H
#define __KERNEL_FROGFS_ROOT_H

struct block_device;

enum frogfs_root_status {
        FROGFS_ROOT_FOUND = 0,
        FROGFS_ROOT_NOT_FOUND,
        FROGFS_ROOT_DUPLICATE,
        FROGFS_ROOT_CORRUPT,
        FROGFS_ROOT_UNREADABLE,
};

struct frogfs_root_result {
        enum frogfs_root_status status;
        struct block_device *bdev;
};

/*
 * Locate the reserved frog-root volume among all published partitions.
 * Exactly one structurally valid match is required.  Two valid matches return
 * DUPLICATE even if another partition is unreadable.  A readable damaged
 * candidate whose fixed label is frog-root returns CORRUPT; otherwise any
 * unreadable partition makes the result UNREADABLE because a second root
 * cannot be ruled out.  bdev is non-NULL only for FOUND.
 *
 * A returned bdev is owned by the block registry and remains valid for the
 * current boot; callers must not free it.
 */
struct frogfs_root_result frogfs_locate_root(void);

#ifdef CONFIG_FROG_TEST_ROOT_LOCATOR
void frogfs_root_locator_regression_run(void);
#endif

#endif
