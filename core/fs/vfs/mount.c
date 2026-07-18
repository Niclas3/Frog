#include <frog/errno.h>
#include <frog/list.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/vfs.h>


static struct list_head mount_list;
static struct list_head fs_type_list;

void init_mount_list(void)
{
        INIT_LIST_HEAD(&mount_list);
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
