#include <frog/block.h>
#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/assert.h>
#include <kernel/chardev.h>
#include <kernel/debug.h>
#include <kernel/dev.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>

#define DEVFS_MAGIC 0xffaabbee

struct dentry *devfs_lookup(struct inode *dir, struct dentry *target);
static struct inode_operations devfs_iop = {
    .lookup = devfs_lookup,
};

static struct inode *devfs_alloc_block_inode(
    struct inode *parent,
    struct block_device_operations *bop,
    int_32 dev_no)
{
        struct inode *dev_inode = kmalloc(sizeof(struct inode));
        if (!dev_inode)
                return NULL;
        dev_inode->i_sb = parent->i_sb;
        dev_inode->i_bdop = bop;
        dev_inode->i_dev = dev_no;

        return dev_inode;
}

static struct inode *devfs_alloc_inode(struct inode *parent,
                                       struct file_operations *fop)
{
        struct inode *dev_inode = kmalloc(sizeof(struct inode));
        if (!dev_inode)
                return NULL;
        dev_inode->i_sb = parent->i_sb;
        dev_inode->i_fop = fop;

        return dev_inode;
}

// /dev/input/event0

struct dentry *devfs_lookup(struct inode *dir, struct dentry *target)
{
        struct dentry *devfs_mount_point = find_mount_entry("dev")->mount_point;
        // return a dentry with target char driver fop inode
        /* ASSERT(devfs_mount_point); */
        DEBUG("[devfs]:look up %s", target->d_name);
        /* char *name ; */
        /* struct dentry *d = kmalloc(sizeof(struct dentry)); */
        /* d->d_inode = kmalloc(sizeof(struct inode)); */

        /* struct file_operations *fop =  */

        return NULL;
}
struct dentry *make_virtual_node(struct dentry *current, char *component)
{
        struct dentry *d = kmalloc(sizeof(struct dentry));
        if (!d)
                return NULL;
        INIT_LIST_HEAD(&d->d_subdirs);
        d->d_parent = current;
        d->d_name = kmalloc(FILE_NAME_MAX);
        ASSERT(d->d_name);
        strcpy(d->d_name, component);
        d->d_type = FT_DIRECTORY;
        list_add_tail(&d->d_child_node, &current->d_subdirs);
        d->d_inode = devfs_alloc_inode(current->d_inode, NULL);
        return d;
}

struct dentry *make_dev_node(struct dentry *current,
                             char *component,
                             int type,
                             int dev_no)
{
        struct dentry *d = kmalloc(sizeof(struct dentry));
        if (!d)
                return NULL;
        int major = DEV_MAJOR(dev_no);
        d->d_parent = current;
        d->d_name = kmalloc(FILE_NAME_MAX);
        ASSERT(d->d_name);
        strcpy(d->d_name, component);
        list_add_tail(&d->d_child_node, &current->d_subdirs);

        if (type == DEV_TYPE_CHAR) {
                d->d_type = FT_CHAR;
                const struct file_operations *devfop = get_chardev_fop(major);
                ASSERT(devfop);
                struct inode *newi =
                    devfs_alloc_inode(current->d_inode, devfop);
                d->d_inode = newi;

        } else if (type == DEV_TYPE_BLOCK) {
                d->d_type = FT_BLOCK;
                const struct block_device_operations *bdops =
                    get_blkdev_operations(major);
                ASSERT(bdops);
                struct inode *newi =
                    devfs_alloc_block_inode(current->d_inode, bdops, dev_no);
                d->d_inode = newi;
        } else {
                PANIC("[devfs]: Unknow device type ");
        }

        return d;
}

int devfs_create_node(char *pathname, int type, int major, int minor)
{
        ASSERT(pathname[0] != '/');
        struct dentry *current = find_mount_entry("dev")->mount_point;
        if (!current) {
                DEBUG("[devfs]: can not find dev mount entry when create node");
                return -1;
        }
        dev_t dev_no = DEV_NR(major, minor);
        char *component = kmalloc(FILE_NAME_MAX);
        int path_len = strlen(pathname);
        char *path = kmalloc(path_len);
        char **p_path = &path;
        strncpy(path, pathname, path_len);

        p_path = next_path_components(p_path, component);

        do {
                struct dentry *res = dentry_lookup(current, component);
                if (!res) {
                        // node not existed create a dentry node
                        // if component is last component
                        // not last component
                        if (*p_path == NULL) {
                                struct dentry *last = make_dev_node(
                                    current, component, type, dev_no);
                                if (!last)
                                        goto create_node_fail;
                                ASSERT(last);
                                current = last;
                        } else {
                                // make virtual node
                                struct dentry *node =
                                    make_virtual_node(current, component);
                                if (!node)
                                        goto create_node_fail;
                                ASSERT(node);
                                current = node;
                        }
                } else {
                        current = res;
                }
                p_path = next_path_components(p_path, component);
        } while (strcmp(component, ""));

        kfree(component);
        kfree(path);
        return 0;
create_node_fail:
        kfree(component);
        kfree(path);
        return -1;
}

static struct inode *devfs_create_root_inode(struct super_block *sb)
{
        struct inode *dev_inode = kmalloc(sizeof(struct inode));
        if (!dev_inode)
                return NULL;
        dev_inode->i_sb = sb;
        dev_inode->i_dev = 0;
        dev_inode->i_op = &devfs_iop;

        return dev_inode;
}

static struct super_block *devfs_mount(struct fs_type *fs,
                                       int flags,
                                       const char *dev,
                                       void *data)
{
        struct super_block *devsb = kmalloc(sizeof(struct super_block));
        if (!devsb)
                return NULL;

        devsb->s_devno = 0;
        devsb->s_dev = NULL;
        devsb->s_magic = DEVFS_MAGIC;
        devsb->s_fs_info = NULL;
        INIT_LIST_HEAD(&devsb->s_inodes);

        struct inode *root_inode = devfs_create_root_inode(devsb);
        if (!root_inode)
                return NULL;

        devsb->s_root = root_inode;
        return devsb;
}

static struct fs_type devfs_type = {.name = "devfs", .mount = devfs_mount};

extern struct dentry *global_root_dentry;
static int make_mount_point(struct dentry *root, char *path)
{
        struct dentry *child = kmalloc(sizeof(struct dentry));
        if (!child)
                return -1;
        child->d_name = "dev";
        child->d_parent = root;
        child->d_mounted = false;
        child->d_type = FT_DIRECTORY;
        child->d_inode = kmalloc(sizeof(struct inode));
        INIT_LIST_HEAD(&child->d_subdirs);
        list_add_tail(&child->d_child_node, &root->d_subdirs);

        if (vfs_mkdir(global_root_dentry, child) == -1) {
                return -1;
        }
        return 0;
}


int dev_fs_init(void)
{
        register_fs(&devfs_type);

        make_mount_point(global_root_dentry, "dev");
        vfs_mount("/dev", "devfs", 0, NULL, NULL);
        return 0;
}
