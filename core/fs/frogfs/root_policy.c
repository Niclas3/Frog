#include <frog/errno.h>
#include <kernel/frogfs.h>
#include <kernel/root_switch.h>
#include <kernel/system_root.h>
#include <kernel/vfs.h>

static struct frogfs_root_activation activation_failure(
    enum frogfs_root_activation_status status,
    struct block_device *bdev,
    struct super_block *sb,
    int error,
    int cleanup_error)
{
        struct frogfs_root_activation result = {
            .status = status,
            .bdev = bdev,
            .sb = sb,
            .error = error,
            .cleanup_error = cleanup_error,
        };
        return result;
}

struct frogfs_root_activation frogfs_activate_root(
    const struct frogfs_root_result *located)
{
        struct block_device *bdev;
        struct dentry *mounted_root;
        struct super_block *sb;
        int error;

        if (!located || located->status != FROGFS_ROOT_FOUND ||
            !located->bdev)
                return activation_failure(FROGFS_ROOT_ACTIVATE_BAD_RESULT,
                                          NULL, NULL, -EINVAL, 0);
        bdev = located->bdev;

        error = frogfs_init();
        if (error < 0)
                return activation_failure(
                    FROGFS_ROOT_ACTIVATE_REGISTER_FAILED, bdev, NULL,
                    error, 0);
        error = vfs_mkdir_path("/sysroot");
        if (error < 0) {
                int cleanup_error = frogfs_init_rollback();
                return activation_failure(FROGFS_ROOT_ACTIVATE_MKDIR_FAILED,
                                          bdev, NULL, error, cleanup_error);
        }
        error = vfs_mount_block("/sysroot", "frogfs", 0, bdev, NULL);
        if (error < 0) {
                int cleanup_error = vfs_rmdir_path("/sysroot");
                int unregister_error = frogfs_init_rollback();
                if (cleanup_error == 0)
                        cleanup_error = unregister_error;
                return activation_failure(FROGFS_ROOT_ACTIVATE_MOUNT_FAILED,
                                          bdev, NULL, error, cleanup_error);
        }

        mounted_root = vfs_lookup("/sysroot");
        sb = mounted_root ? mounted_root->d_sb : NULL;
        if (!sb || sb->s_bdev != bdev)
                return activation_failure(
                    FROGFS_ROOT_ACTIVATE_SOURCE_MISMATCH, bdev, sb, -EIO, 0);
        if (!frogfs_super_is_read_only(sb))
                return activation_failure(FROGFS_ROOT_ACTIVATE_NOT_READ_ONLY,
                                          bdev, sb, -EROFS, 0);

        error = vfs_switch_root_once();
        if (error < 0)
                return activation_failure(FROGFS_ROOT_ACTIVATE_SWITCH_FAILED,
                                          bdev, sb, error, 0);
        return activation_failure(FROGFS_ROOT_ACTIVATED, bdev, sb, 0, 0);
}
