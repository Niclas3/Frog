#ifndef __FROG_KERNEL_ROOT_SWITCH_H
#define __FROG_KERNEL_ROOT_SWITCH_H

#include <frog/types.h>

/*
 * Commit the boot namespace exactly once.  The staged root must already be
 * mounted at /sysroot and contain an unmounted /dev directory.  The existing
 * devfs mount at /dev, including its nested packagefs mount, is rebound there
 * without recreating either filesystem.
 *
 * Success transfers no caller-owned reference: VFS pins both the new root and
 * retired Bootstrap Root for this boot.  A second call returns -EALREADY.
 * Before the commit point, missing/type/device/topology/invariant failures are
 * reported as -ENOENT, -ENOTDIR, -ENODEV, -EBUSY, or -EUCLEAN and leave the
 * old namespace unchanged.  This zero-allocation operation does not return
 * -ENOMEM.  After all validation, the locked commit is infallible and the
 * final global-root store is its linearization point.
 */
int_32 vfs_switch_root_once(void);

#ifdef CONFIG_FROG_TEST_ROOT_SWITCH
#define VFS_ROOT_SWITCH_TEST_SNAPSHOT_WORDS 32U

struct vfs_root_switch_test_snapshot {
        uint_32 opaque[VFS_ROOT_SWITCH_TEST_SNAPSHOT_WORDS];
};

enum vfs_root_switch_test_checkpoint {
        VFS_ROOT_SWITCH_TEST_FAIL_NONE = 0,
        VFS_ROOT_SWITCH_TEST_FAIL_STAGED_ROOT,
        VFS_ROOT_SWITCH_TEST_FAIL_NEW_DEV,
        VFS_ROOT_SWITCH_TEST_FAIL_OLD_ROOT,
        VFS_ROOT_SWITCH_TEST_FAIL_DEVFS,
        VFS_ROOT_SWITCH_TEST_FAIL_PACKAGEFS,
        VFS_ROOT_SWITCH_TEST_FAIL_TOPOLOGY,
};

void vfs_root_switch_test_fail_at(
    enum vfs_root_switch_test_checkpoint checkpoint);
bool vfs_root_switch_test_snapshot(
    struct vfs_root_switch_test_snapshot *snapshot);
bool vfs_root_switch_test_snapshot_unchanged(
    const struct vfs_root_switch_test_snapshot *snapshot);
bool vfs_root_switch_test_snapshot_committed(
    const struct vfs_root_switch_test_snapshot *snapshot);
void vfs_root_switch_regression_run(void);
#endif

#endif
