#include <frog/block.h>
#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/chardev.h>
#include <kernel/dev.h>
#include <kernel/vfs.h>

#define DEVFS_MAGIC 0xffaabbee

static struct dentry *devfs_lookup(struct inode *dir,
                                   struct dentry *target);
static struct inode_operations devfs_iop = {
    .lookup = devfs_lookup,
};

static struct inode *devfs_alloc_block_inode(
    struct inode *parent,
    struct block_device_operations *bop,
    dev_t dev_no)
{
        struct inode *dev_inode = kmalloc(sizeof(struct inode));
        if (!dev_inode)
                return NULL;
        memset(dev_inode, 0, sizeof(*dev_inode));
        dev_inode->i_sb = parent->i_sb;
        dev_inode->i_bdop = bop;
        dev_inode->i_dev = dev_no;
        dev_inode->i_mode = FT_BLOCK << 11;
        dev_inode->i_nlink = 1;
        INIT_LIST_HEAD(&dev_inode->i_active_node);

        return dev_inode;
}

static struct inode *devfs_alloc_inode(struct inode *parent,
                                       struct file_operations *fop,
                                       enum file_type type)
{
        struct inode *dev_inode = kmalloc(sizeof(struct inode));
        if (!dev_inode)
                return NULL;
        memset(dev_inode, 0, sizeof(*dev_inode));
        dev_inode->i_sb = parent->i_sb;
        dev_inode->i_fop = fop;
        dev_inode->i_mode = type << 11;
        dev_inode->i_nlink = type == FT_DIRECTORY ? 2 : 1;
        INIT_LIST_HEAD(&dev_inode->i_active_node);

        return dev_inode;
}

// /dev/input/event0

static struct dentry *devfs_lookup(struct inode *dir,
                                   struct dentry *target)
{
        if (!dir || !target || !target->d_name || !target->d_parent)
                return NULL;

        return dentry_lookup(target->d_parent, target->d_name);
}
static struct dentry *make_virtual_node(struct dentry *current,
                                        const char *component)
{
        struct dentry *d = kmalloc(sizeof(struct dentry));
        if (!d)
                return NULL;
        memset(d, 0, sizeof(*d));
        INIT_LIST_HEAD(&d->d_subdirs);
        INIT_LIST_HEAD(&d->d_child_node);
        d->d_parent = current;
        d->d_name = kmalloc(strlen(component) + 1);
        if (!d->d_name) {
                kfree(d);
                return NULL;
        }
        strcpy(d->d_name, component);
        d->d_type = FT_DIRECTORY;
        d->d_inode =
            devfs_alloc_inode(current->d_inode, NULL, FT_DIRECTORY);
        if (!d->d_inode) {
                kfree(d->d_name);
                kfree(d);
                return NULL;
        }
        d->d_inode->i_op = &devfs_iop;
        d->d_sb = current->d_sb;
        dentry_add_child(current, d);
        return d;
}

static struct dentry *make_dev_node(struct dentry *current,
                                    const char *component,
                                    int type,
                                    dev_t dev_no)
{
        struct dentry *d = kmalloc(sizeof(struct dentry));
        if (!d)
                return NULL;
        memset(d, 0, sizeof(*d));
        int major = DEV_MAJOR(dev_no);
        d->d_parent = current;
        d->d_name = kmalloc(strlen(component) + 1);
        if (!d->d_name) {
                kfree(d);
                return NULL;
        }
        strcpy(d->d_name, component);
        INIT_LIST_HEAD(&d->d_subdirs);
        INIT_LIST_HEAD(&d->d_child_node);
        d->d_sb = current->d_sb;

        if (type == DEV_TYPE_CHAR) {
                d->d_type = FT_CHAR;
                const struct file_operations *devfop = get_chardev_fop(major);
                if (!devfop)
                        goto create_fail;
                struct inode *newi =
                    devfs_alloc_inode(current->d_inode,
                                      (struct file_operations *) devfop,
                                      FT_CHAR);
                if (!newi)
                        goto create_fail;
                newi->i_dev = dev_no;
                d->d_inode = newi;

        } else if (type == DEV_TYPE_BLOCK) {
                d->d_type = FT_BLOCK;
                const struct block_device_operations *bdops =
                    get_blkdev_operations(major);
                if (!bdops)
                        goto create_fail;
                struct inode *newi =
                    devfs_alloc_block_inode(
                        current->d_inode,
                        (struct block_device_operations *) bdops, dev_no);
                if (!newi)
                        goto create_fail;
                d->d_inode = newi;
        } else {
                goto create_fail;
        }

        dentry_add_child(current, d);
        return d;
create_fail:
        kfree(d->d_name);
        kfree(d);
        return NULL;
}

static bool devfs_valid_path(const char *path)
{
        if (!path || !path[0])
                return false;
        uint_32 path_len = 0;
        while (path_len <= PATH_NAME_MAX && path[path_len])
                path_len++;
        if (!path_len || path_len > PATH_NAME_MAX || path[0] == '/' ||
            path[path_len - 1] == '/')
                return false;

        uint_32 component_len = 0;
        const char *component = path;
        for (uint_32 i = 0;; i++) {
                if (path[i] != '/' && path[i] != '\0') {
                        if (++component_len > FILE_NAME_MAX)
                                return false;
                        continue;
                }
                if (!component_len)
                        return false;
                if ((component_len == 1 && component[0] == '.') ||
                    (component_len == 2 && component[0] == '.' &&
                     component[1] == '.'))
                        return false;
                if (path[i] == '\0')
                        return true;
                component = path + i + 1;
                component_len = 0;
        }
}

static void devfs_destroy_subtree(struct dentry *root)
{
        while (!list_is_empty(&root->d_subdirs)) {
                struct dentry *child = container_of(
                    root->d_subdirs.next, struct dentry, d_child_node);
                devfs_destroy_subtree(child);
        }
        list_del(&root->d_child_node);
        kfree(root->d_inode);
        kfree(root->d_name);
        kfree(root);
}

int devfs_create_node(const char *pathname, int type, int major, int minor)
{
        if (!devfs_valid_path(pathname) ||
            (type != DEV_TYPE_CHAR && type != DEV_TYPE_BLOCK) ||
            major < 0 || major >= 255 || minor < 0 || minor > 0xffff)
                return -EINVAL;
        if ((type == DEV_TYPE_CHAR && !get_chardev_fop(major)) ||
            (type == DEV_TYPE_BLOCK && !get_blkdev_operations(major)))
                return -ENODEV;

        vfs_namespace_lock();
        struct mount_entry *mount = find_mount_entry("devfs");
        if (!mount || !mount->mounted_root) {
                vfs_namespace_unlock();
                return -ENODEV;
        }
        struct dentry *current = mount->mounted_root;
        struct dentry *first_created = NULL;
        dev_t dev_no = DEV_NR(major, minor);
        const char *cursor = pathname;
        int ret = 0;

        for (;;) {
                const char *end = cursor;
                while (*end && *end != '/')
                        end++;
                uint_32 component_len = end - cursor;
                char component[FILE_NAME_MAX + 1];
                memcpy(component, cursor, component_len);
                component[component_len] = '\0';
                bool last_component = *end == '\0';

                struct dentry *res = dentry_lookup(current, component);
                if (!res) {
                        if (last_component) {
                                res = make_dev_node(
                                    current, component, type, dev_no);
                        } else {
                                res = make_virtual_node(current, component);
                        }
                        if (!res) {
                                ret = -ENOMEM;
                                goto create_node_fail;
                        }
                        if (!first_created)
                                first_created = res;
                } else {
                        if (!last_component &&
                            res->d_type != FT_DIRECTORY) {
                                ret = -ENOTDIR;
                                goto create_node_fail;
                        }
                        if (last_component) {
                                if (!res->d_inode || res->d_type !=
                                                         (type == DEV_TYPE_CHAR
                                                              ? FT_CHAR
                                                              : FT_BLOCK) ||
                                    res->d_inode->i_dev != dev_no) {
                                        ret = -EEXIST;
                                        goto create_node_fail;
                                }
                        }
                }
                current = res;
                if (last_component)
                        break;
                cursor = end + 1;
        }

        vfs_namespace_unlock();
        return 0;
create_node_fail:
        if (first_created)
                devfs_destroy_subtree(first_created);
        vfs_namespace_unlock();
        return ret;
}

static struct inode *devfs_create_root_inode(struct super_block *sb)
{
        struct inode *dev_inode = kmalloc(sizeof(struct inode));
        if (!dev_inode)
                return NULL;
        memset(dev_inode, 0, sizeof(*dev_inode));
        dev_inode->i_sb = sb;
        dev_inode->i_dev = 0;
        dev_inode->i_op = &devfs_iop;
        dev_inode->i_mode = FT_DIRECTORY << 11;
        dev_inode->i_nlink = 2;
        INIT_LIST_HEAD(&dev_inode->i_active_node);

        return dev_inode;
}

static struct super_block *devfs_mount(struct fs_type *fs,
                                       int flags,
                                       const char *dev,
                                       void *data)
{
        (void) fs;
        (void) flags;
        (void) dev;
        (void) data;
        struct super_block *devsb = kmalloc(sizeof(struct super_block));
        if (!devsb)
                return NULL;

        memset(devsb, 0, sizeof(*devsb));
        devsb->s_devno = 0;
        devsb->s_dev = NULL;
        devsb->s_magic = DEVFS_MAGIC;
        devsb->s_fs_info = NULL;
        INIT_LIST_HEAD(&devsb->s_inodes);

        struct inode *root_inode = devfs_create_root_inode(devsb);
        if (!root_inode) {
                kfree(devsb);
                return NULL;
        }

        devsb->s_root = root_inode;
        return devsb;
}

static struct fs_type devfs_type = {.name = "devfs", .mount = devfs_mount};

int dev_fs_init(void)
{
        int ret = register_fs(&devfs_type);
        if (ret < 0)
                return ret;
        ret = vfs_mkdir_path("/dev");
        if (ret < 0) {
                unregister_fs(&devfs_type);
                return ret;
        }
        ret = vfs_mount("/dev", "devfs", 0, NULL, NULL);
        if (ret < 0) {
                vfs_rmdir_path("/dev");
                unregister_fs(&devfs_type);
        }
        return ret;
}
