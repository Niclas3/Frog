#include <frog/errno.h>
#include <frog/list.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/root_switch.h>
#include <kernel/vfs.h>


static struct list_head mount_list;
static struct list_head fs_type_list;
extern struct dentry *global_root_dentry;
static struct mount_entry *retired_root_mount;
static bool root_switch_committed;

#ifdef CONFIG_FROG_TEST_ROOT_SWITCH
static enum vfs_root_switch_test_checkpoint root_switch_fail_checkpoint;
#endif

struct root_switch_plan {
        struct dentry *old_root;
        struct dentry *staged_root;
        struct dentry *old_sysroot;
        struct dentry *old_dev;
        struct dentry *new_dev;
        struct dentry *devfs_root;
        struct dentry *packagefs_root;
        struct mount_entry *old_root_entry;
        struct mount_entry *staged_entry;
        struct mount_entry *devfs_entry;
        struct mount_entry *packagefs_entry;
};

enum root_switch_checkpoint {
        ROOT_SWITCH_FAIL_STAGED_ROOT = 1,
        ROOT_SWITCH_FAIL_NEW_DEV,
        ROOT_SWITCH_FAIL_OLD_ROOT,
        ROOT_SWITCH_FAIL_DEVFS,
        ROOT_SWITCH_FAIL_PACKAGEFS,
        ROOT_SWITCH_FAIL_TOPOLOGY,
};

#ifdef CONFIG_FROG_TEST_ROOT_SWITCH
struct root_switch_snapshot_internal {
        struct dentry *global_root;
        struct mount_entry *entries[4];
        struct dentry *mount_points[4];
        struct dentry *mounted_roots[4];
        struct super_block *super_blocks[4];
        struct dentry *super_mount_points[4];
        struct dentry *old_sysroot;
        struct dentry *old_dev;
        struct dentry *new_dev;
        uint_32 old_sysroot_mounted;
        uint_32 old_dev_mounted;
        uint_32 new_dev_mounted;
        struct mount_entry *active_order[4];
        uint_32 active_count;
};

typedef char root_switch_snapshot_must_fit[
    sizeof(struct root_switch_snapshot_internal) <=
            sizeof(struct vfs_root_switch_test_snapshot)
        ? 1
        : -1];
#endif

static struct mount_entry *find_mount_by_root_unlocked(
    struct dentry *mounted_root, bool *duplicate)
{
        struct mount_entry *found = NULL;
        struct list_head *position;

        *duplicate = false;
        list_for_each(position, &mount_list) {
                struct mount_entry *entry =
                    container_of(position, struct mount_entry, mount_node);

                if (entry->mounted_root != mounted_root)
                        continue;
                if (found != NULL) {
                        *duplicate = true;
                        return NULL;
                }
                found = entry;
        }
        return found;
}

static struct mount_entry *find_mount_by_fs_unlocked(
    const char *fs_name, bool *duplicate)
{
        struct mount_entry *found = NULL;
        struct list_head *position;

        *duplicate = false;
        list_for_each(position, &mount_list) {
                struct mount_entry *entry =
                    container_of(position, struct mount_entry, mount_node);

                if (entry->fs_name == NULL || strcmp(entry->fs_name, fs_name))
                        continue;
                if (found != NULL) {
                        *duplicate = true;
                        return NULL;
                }
                found = entry;
        }
        return found;
}

static bool mount_entry_active_unlocked(struct mount_entry *entry)
{
        return entry != NULL &&
               list_find_element(&entry->mount_node, &mount_list);
}

static uint_32 mount_count_unlocked(void)
{
        struct list_head *position;
        uint_32 count = 0;

        list_for_each(position, &mount_list)
                count++;
        return count;
}

static bool mount_entry_consistent_unlocked(struct mount_entry *entry)
{
        return entry != NULL && entry->fs_name != NULL &&
               entry->mount_point != NULL && entry->mounted_root != NULL &&
               entry->sb != NULL && entry->sb->s_root != NULL &&
               entry->sb->s_mountpoint == entry->mount_point &&
               entry->mounted_root->d_parent == entry->mounted_root &&
               entry->mounted_root->d_inode == entry->sb->s_root &&
               entry->mounted_root->d_sb == entry->sb &&
               entry->mounted_root->d_type == FT_DIRECTORY;
}

#ifdef CONFIG_FROG_TEST_ROOT_SWITCH
static int root_switch_inject_unlocked(int checkpoint)
{
        if ((int) root_switch_fail_checkpoint != checkpoint)
                return 0;
        root_switch_fail_checkpoint = VFS_ROOT_SWITCH_TEST_FAIL_NONE;
        return -EIO;
}
#define ROOT_SWITCH_MAYBE_INJECT(allow, checkpoint) \
        ((allow) ? root_switch_inject_unlocked(checkpoint) : 0)
#else
#define ROOT_SWITCH_MAYBE_INJECT(allow, checkpoint) (0)
#endif

static int root_switch_prepare_unlocked(struct root_switch_plan *plan,
                                        bool allow_injection)
{
        bool duplicate;
        struct dentry *new_dev_path;
        int result;

#ifndef CONFIG_FROG_TEST_ROOT_SWITCH
        (void) allow_injection;
#endif
        memset(plan, 0, sizeof(*plan));
        plan->old_root = global_root_dentry;
        plan->staged_root = vfs_lookup("/sysroot");
        if (plan->staged_root == NULL)
                return -ENOENT;
        if (plan->staged_root->d_type != FT_DIRECTORY)
                return -ENOTDIR;
        plan->staged_entry =
            find_mount_by_root_unlocked(plan->staged_root, &duplicate);
        if (duplicate || !mount_entry_consistent_unlocked(
                             plan->staged_entry))
                return -EUCLEAN;
        plan->old_sysroot = plan->staged_entry->mount_point;
        if (plan->old_sysroot == plan->staged_root ||
            !plan->old_sysroot->d_mounted)
                return -EBUSY;
        result = ROOT_SWITCH_MAYBE_INJECT(
            allow_injection, ROOT_SWITCH_FAIL_STAGED_ROOT);
        if (result != 0)
                return result;

        new_dev_path = vfs_lookup("/sysroot/dev");
        plan->new_dev = dentry_lookup(plan->staged_root, "dev");
        if (new_dev_path == NULL || plan->new_dev == NULL ||
            new_dev_path != plan->new_dev)
                return -ENOENT;
        if (plan->new_dev->d_type != FT_DIRECTORY)
                return -ENOTDIR;
        if (plan->new_dev->d_mounted)
                return -EBUSY;
        result = ROOT_SWITCH_MAYBE_INJECT(
            allow_injection, ROOT_SWITCH_FAIL_NEW_DEV);
        if (result != 0)
                return result;

        plan->old_root_entry =
            find_mount_by_root_unlocked(plan->old_root, &duplicate);
        if (duplicate || !mount_entry_consistent_unlocked(
                             plan->old_root_entry) ||
            plan->old_root_entry->mount_point != plan->old_root ||
            strcmp(plan->old_root_entry->fs_name, "rootfs"))
                return -EUCLEAN;
        result = ROOT_SWITCH_MAYBE_INJECT(
            allow_injection, ROOT_SWITCH_FAIL_OLD_ROOT);
        if (result != 0)
                return result;

        plan->devfs_entry = find_mount_by_fs_unlocked("devfs", &duplicate);
        if (duplicate)
                return -EUCLEAN;
        if (plan->devfs_entry == NULL)
                return -ENODEV;
        if (!mount_entry_consistent_unlocked(plan->devfs_entry))
                return -EUCLEAN;
        plan->old_dev = plan->devfs_entry->mount_point;
        plan->devfs_root = plan->devfs_entry->mounted_root;
        if (plan->old_dev->d_parent != plan->old_root ||
            plan->old_dev->d_name == NULL ||
            strcmp(plan->old_dev->d_name, "dev") ||
            !plan->old_dev->d_mounted ||
            vfs_lookup("/dev") != plan->devfs_root)
                return -EUCLEAN;
        result = ROOT_SWITCH_MAYBE_INJECT(
            allow_injection, ROOT_SWITCH_FAIL_DEVFS);
        if (result != 0)
                return result;

        plan->packagefs_entry =
            find_mount_by_fs_unlocked("packagefs", &duplicate);
        if (duplicate)
                return -EUCLEAN;
        if (plan->packagefs_entry == NULL)
                return -ENODEV;
        if (!mount_entry_consistent_unlocked(plan->packagefs_entry))
                return -EUCLEAN;
        plan->packagefs_root = plan->packagefs_entry->mounted_root;
        if (plan->packagefs_entry->mount_point->d_parent !=
                plan->devfs_root ||
            plan->packagefs_entry->mount_point->d_name == NULL ||
            strcmp(plan->packagefs_entry->mount_point->d_name, "pkg") ||
            !plan->packagefs_entry->mount_point->d_mounted ||
            vfs_lookup("/dev/pkg") != plan->packagefs_root)
                return -EUCLEAN;
        result = ROOT_SWITCH_MAYBE_INJECT(
            allow_injection, ROOT_SWITCH_FAIL_PACKAGEFS);
        if (result != 0)
                return result;

        if (retired_root_mount != NULL || plan->old_root == NULL ||
            plan->old_root == plan->staged_root ||
            plan->old_sysroot->d_parent != plan->old_root ||
            plan->old_sysroot->d_name == NULL ||
            strcmp(plan->old_sysroot->d_name, "sysroot") ||
            plan->new_dev->d_parent != plan->staged_root ||
            plan->new_dev->d_sb != plan->staged_entry->sb ||
            plan->old_root_entry == plan->staged_entry ||
            plan->old_root_entry == plan->devfs_entry ||
            plan->old_root_entry == plan->packagefs_entry ||
            plan->staged_entry == plan->devfs_entry ||
            plan->staged_entry == plan->packagefs_entry ||
            plan->devfs_entry == plan->packagefs_entry ||
            !mount_entry_active_unlocked(plan->old_root_entry) ||
            !mount_entry_active_unlocked(plan->staged_entry) ||
            !mount_entry_active_unlocked(plan->devfs_entry) ||
            !mount_entry_active_unlocked(plan->packagefs_entry) ||
            mount_count_unlocked() != 4U)
                return -EBUSY;
        return ROOT_SWITCH_MAYBE_INJECT(
            allow_injection, ROOT_SWITCH_FAIL_TOPOLOGY);
}

int_32 vfs_switch_root_once(void)
{
        struct root_switch_plan plan;
        int result;

        vfs_namespace_lock();
        if (root_switch_committed) {
                vfs_namespace_unlock();
                return -EALREADY;
        }
        result = root_switch_prepare_unlocked(&plan, true);
        if (result != 0) {
                vfs_namespace_unlock();
                return result;
        }

        /*
         * No operation below can fail.  namespace_lock hides the intermediate
         * rewiring; publishing global_root_dentry last is the linearization
         * point observed by subsequent path lookup.
         */
        plan.old_sysroot->d_mounted = false;
        plan.old_dev->d_mounted = false;
        plan.new_dev->d_mounted = true;

        plan.devfs_entry->mount_point = plan.new_dev;
        plan.devfs_entry->sb->s_mountpoint = plan.new_dev;

        plan.staged_root->d_name = "/";
        plan.staged_root->d_parent = plan.staged_root;
        plan.staged_entry->mount_point = plan.staged_root;
        plan.staged_entry->mounted_root = plan.staged_root;
        plan.staged_entry->sb->s_mountpoint = plan.staged_root;

        list_del_init(&plan.old_root_entry->mount_node);
        retired_root_mount = plan.old_root_entry;
        root_switch_committed = true;
        global_root_dentry = plan.staged_root;
        vfs_namespace_unlock();
        return 0;
}

#ifdef CONFIG_FROG_TEST_ROOT_SWITCH
static struct root_switch_snapshot_internal *root_switch_snapshot_mutable(
    struct vfs_root_switch_test_snapshot *snapshot)
{
        return (struct root_switch_snapshot_internal *) snapshot->opaque;
}

static const struct root_switch_snapshot_internal *
root_switch_snapshot_const(
    const struct vfs_root_switch_test_snapshot *snapshot)
{
        return (const struct root_switch_snapshot_internal *)
            snapshot->opaque;
}

static bool root_switch_capture_active_order_unlocked(
    struct root_switch_snapshot_internal *snapshot)
{
        struct list_head *position;

        snapshot->active_count = 0;
        list_for_each(position, &mount_list) {
                if (snapshot->active_count >= 4U)
                        return false;
                snapshot->active_order[snapshot->active_count++] =
                    container_of(position, struct mount_entry, mount_node);
        }
        return snapshot->active_count == 4U;
}

void vfs_root_switch_test_fail_at(
    enum vfs_root_switch_test_checkpoint checkpoint)
{
        vfs_namespace_lock();
        if (checkpoint < VFS_ROOT_SWITCH_TEST_FAIL_NONE ||
            checkpoint > VFS_ROOT_SWITCH_TEST_FAIL_TOPOLOGY)
                checkpoint = VFS_ROOT_SWITCH_TEST_FAIL_NONE;
        root_switch_fail_checkpoint = checkpoint;
        vfs_namespace_unlock();
}

bool vfs_root_switch_test_snapshot(
    struct vfs_root_switch_test_snapshot *opaque)
{
        struct root_switch_snapshot_internal *snapshot;
        struct root_switch_plan plan;
        bool captured = false;

        if (opaque == NULL)
                return false;
        memset(opaque, 0, sizeof(*opaque));
        snapshot = root_switch_snapshot_mutable(opaque);

        vfs_namespace_lock();
        if (root_switch_committed ||
            root_switch_prepare_unlocked(&plan, false) != 0)
                goto out;
        snapshot->global_root = plan.old_root;
        snapshot->entries[0] = plan.old_root_entry;
        snapshot->entries[1] = plan.staged_entry;
        snapshot->entries[2] = plan.devfs_entry;
        snapshot->entries[3] = plan.packagefs_entry;
        for (uint_32 index = 0; index < 4U; index++) {
                snapshot->mount_points[index] =
                    snapshot->entries[index]->mount_point;
                snapshot->mounted_roots[index] =
                    snapshot->entries[index]->mounted_root;
                snapshot->super_blocks[index] = snapshot->entries[index]->sb;
                snapshot->super_mount_points[index] =
                    snapshot->entries[index]->sb->s_mountpoint;
        }
        snapshot->old_sysroot = plan.old_sysroot;
        snapshot->old_dev = plan.old_dev;
        snapshot->new_dev = plan.new_dev;
        snapshot->old_sysroot_mounted = plan.old_sysroot->d_mounted;
        snapshot->old_dev_mounted = plan.old_dev->d_mounted;
        snapshot->new_dev_mounted = plan.new_dev->d_mounted;
        captured = root_switch_capture_active_order_unlocked(snapshot);
out:
        vfs_namespace_unlock();
        return captured;
}

static bool root_switch_snapshot_entries_unchanged_unlocked(
    const struct root_switch_snapshot_internal *snapshot)
{
        for (uint_32 index = 0; index < 4U; index++) {
                struct mount_entry *entry = snapshot->entries[index];

                if (entry == NULL ||
                    entry->mount_point != snapshot->mount_points[index] ||
                    entry->mounted_root != snapshot->mounted_roots[index] ||
                    entry->sb != snapshot->super_blocks[index] ||
                    entry->sb->s_mountpoint !=
                        snapshot->super_mount_points[index])
                        return false;
        }
        return true;
}

static bool root_switch_active_order_unchanged_unlocked(
    const struct root_switch_snapshot_internal *snapshot)
{
        struct list_head *position;
        uint_32 index = 0;

        list_for_each(position, &mount_list) {
                if (index >= snapshot->active_count ||
                    container_of(position, struct mount_entry, mount_node) !=
                        snapshot->active_order[index])
                        return false;
                index++;
        }
        return index == snapshot->active_count;
}

bool vfs_root_switch_test_snapshot_unchanged(
    const struct vfs_root_switch_test_snapshot *opaque)
{
        const struct root_switch_snapshot_internal *snapshot;
        bool unchanged;

        if (opaque == NULL)
                return false;
        snapshot = root_switch_snapshot_const(opaque);
        vfs_namespace_lock();
        unchanged = !root_switch_committed && retired_root_mount == NULL &&
                    global_root_dentry == snapshot->global_root &&
                    root_switch_snapshot_entries_unchanged_unlocked(
                        snapshot) &&
                    snapshot->old_sysroot->d_mounted ==
                        snapshot->old_sysroot_mounted &&
                    snapshot->old_dev->d_mounted ==
                        snapshot->old_dev_mounted &&
                    snapshot->new_dev->d_mounted ==
                        snapshot->new_dev_mounted &&
                    root_switch_active_order_unchanged_unlocked(snapshot);
        vfs_namespace_unlock();
        return unchanged;
}

static bool root_switch_active_order_committed_unlocked(
    const struct root_switch_snapshot_internal *snapshot)
{
        struct list_head *position;
        uint_32 original = 0;
        uint_32 current = 0;

        list_for_each(position, &mount_list) {
                struct mount_entry *entry =
                    container_of(position, struct mount_entry, mount_node);

                while (original < snapshot->active_count &&
                       snapshot->active_order[original] ==
                           snapshot->entries[0])
                        original++;
                if (original >= snapshot->active_count ||
                    entry != snapshot->active_order[original])
                        return false;
                original++;
                current++;
        }
        while (original < snapshot->active_count &&
               snapshot->active_order[original] == snapshot->entries[0])
                original++;
        return original == snapshot->active_count && current == 3U;
}

bool vfs_root_switch_test_snapshot_committed(
    const struct vfs_root_switch_test_snapshot *opaque)
{
        const struct root_switch_snapshot_internal *snapshot;
        struct mount_entry *old_root;
        struct mount_entry *staged;
        struct mount_entry *devfs;
        struct mount_entry *packagefs;
        bool committed;

        if (opaque == NULL)
                return false;
        snapshot = root_switch_snapshot_const(opaque);
        old_root = snapshot->entries[0];
        staged = snapshot->entries[1];
        devfs = snapshot->entries[2];
        packagefs = snapshot->entries[3];

        vfs_namespace_lock();
        committed = root_switch_committed &&
                    retired_root_mount == old_root &&
                    global_root_dentry == snapshot->mounted_roots[1] &&
                    !mount_entry_active_unlocked(old_root) &&
                    old_root->mount_point == snapshot->mount_points[0] &&
                    old_root->mounted_root == snapshot->mounted_roots[0] &&
                    old_root->sb == snapshot->super_blocks[0] &&
                    old_root->sb->s_mountpoint ==
                        snapshot->super_mount_points[0] &&
                    staged->mount_point == snapshot->mounted_roots[1] &&
                    staged->mounted_root == snapshot->mounted_roots[1] &&
                    staged->sb == snapshot->super_blocks[1] &&
                    staged->sb->s_mountpoint ==
                        snapshot->mounted_roots[1] &&
                    staged->mounted_root->d_parent ==
                        staged->mounted_root &&
                    staged->mounted_root->d_name != NULL &&
                    !strcmp(staged->mounted_root->d_name, "/") &&
                    devfs->mount_point == snapshot->new_dev &&
                    devfs->mounted_root == snapshot->mounted_roots[2] &&
                    devfs->sb == snapshot->super_blocks[2] &&
                    devfs->sb->s_mountpoint == snapshot->new_dev &&
                    packagefs->mount_point == snapshot->mount_points[3] &&
                    packagefs->mounted_root == snapshot->mounted_roots[3] &&
                    packagefs->sb == snapshot->super_blocks[3] &&
                    packagefs->sb->s_mountpoint ==
                        snapshot->super_mount_points[3] &&
                    !snapshot->old_sysroot->d_mounted &&
                    !snapshot->old_dev->d_mounted &&
                    snapshot->new_dev->d_mounted &&
                    root_switch_active_order_committed_unlocked(snapshot);
        vfs_namespace_unlock();
        return committed;
}
#endif

void init_mount_list(void)
{
        INIT_LIST_HEAD(&mount_list);
        retired_root_mount = NULL;
        root_switch_committed = false;
#ifdef CONFIG_FROG_TEST_ROOT_SWITCH
        root_switch_fail_checkpoint = VFS_ROOT_SWITCH_TEST_FAIL_NONE;
#endif
}

void init_fs_type_list(void)
{
        INIT_LIST_HEAD(&fs_type_list);
}

void add_mount_list(struct list_head *node)
{
        vfs_namespace_lock();
        list_add_tail(node, &mount_list);
        vfs_namespace_unlock();
}

struct fs_type *find_fs_type(const char *name)
{
        if (!name)
                return NULL;
        vfs_namespace_lock();
        struct list_head *pos;
        list_for_each (pos, &fs_type_list) {
                struct fs_type *target =
                    container_of(pos, struct fs_type, fs_type_node);
                if (target && !strcmp(target->name, name)) {
                        vfs_namespace_unlock();
                        return target;
                }
        }
        vfs_namespace_unlock();
        return NULL;
}


int register_fs(struct fs_type *fs_type)
{
        vfs_namespace_lock();
        if (!fs_type || !fs_type->name || !fs_type->mount) {
                vfs_namespace_unlock();
                return -EINVAL;
        }
        if (find_fs_type(fs_type->name)) {
                vfs_namespace_unlock();
                return -EEXIST;
        }
        INIT_LIST_HEAD(&fs_type->fs_type_node);
        list_add_tail(&fs_type->fs_type_node, &fs_type_list);
        vfs_namespace_unlock();
        return 0;
}

int unregister_fs(struct fs_type *fs_type)
{
        if (!fs_type)
                return -EINVAL;
        vfs_namespace_lock();
        if (!list_find_element(&fs_type->fs_type_node, &fs_type_list)) {
                vfs_namespace_unlock();
                return -ENOENT;
        }
        list_del_init(&fs_type->fs_type_node);
        vfs_namespace_unlock();
        return 0;
}

static inline boolean __is_same_path_indeed(struct dentry *left, struct dentry *right)
{
        struct dentry *c_l = left;
        struct dentry *c_r = right;
        while (c_l && c_r) {
                if (!c_l->d_name || !c_r->d_name)
                        return false;

                if (strcmp(c_l->d_name, c_r->d_name))
                        return false;

                if (!strcmp(c_l->d_name, "/"))
                        return true;

                if (c_l == c_l->d_parent || c_r == c_r->d_parent)
                        return false;

                c_l = c_l->d_parent;
                c_r = c_r->d_parent;
        }

        return false;
}

static boolean is_same_path(struct dentry *left, struct dentry *right)
{
        if (left == right) {
                return true;
        } else {
                return __is_same_path_indeed(left, right);
        }
}

/**
 * Search dir_other->d_name from mounted_list find real mount point and return
 * it
 *****************************************************************************/
struct dentry *find_mounted_dentry(struct dentry *dir)
{
        vfs_namespace_lock();
        struct list_head *pos;
        list_for_each (pos, &mount_list) {
                struct mount_entry *entry =
                    container_of(pos, struct mount_entry, mount_node);
                ASSERT(entry);
                struct dentry *mp = entry->mount_point;
                if (is_same_path(dir, mp)) {
                        vfs_namespace_unlock();
                        return entry->mounted_root;
                }
        }
        vfs_namespace_unlock();
        return NULL;
}

struct mount_entry *find_mount_entry(const char *mount_point)
{
        vfs_namespace_lock();
        struct list_head *pos;
        list_for_each (pos, &mount_list) {
                struct mount_entry *target =
                    container_of(pos, struct mount_entry, mount_node);
                if (target && target->fs_name &&
                    !strcmp(target->fs_name, mount_point)) {
                        vfs_namespace_unlock();
                        return target;
                }
        }
        vfs_namespace_unlock();
        return NULL;
}
