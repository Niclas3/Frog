#include <frog/errno.h>
#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/debug.h>
#include <kernel/vfs.h>

#define ROOTFS_MAGIC 0xefeefeff

static struct dentry *rootfs_lookup(struct inode *dir,
                                    struct dentry *target);
static int_32 rootfs_mkdir(struct inode *dir,
                           struct dentry *target,
                           uint_32 mode);
static int_32 rootfs_rmdir(struct inode *dir, struct dentry *target);

static struct file_operations rootfs_fops = {0};
static struct inode_operations rootfs_ops = {.lookup = rootfs_lookup,
                                             .mkdir = rootfs_mkdir,
                                             .rmdir = rootfs_rmdir};

extern struct dentry *global_root_dentry;

// TODO: on process
// lookup in `layer_search`
static struct dentry *rootfs_lookup(struct inode *dir,
                                    struct dentry *target)
{
        if (!dir || !target || !target->d_parent || !target->d_name)
                return NULL;
        return dentry_lookup(target->d_parent, target->d_name);
}

static int_32 rootfs_mkdir(struct inode *dir,
                           struct dentry *target,
                           uint_32 mode)
{
        (void) mode;
        if (!dir || !target || !target->d_name || target->d_inode)
                return -EINVAL;
        if (dentry_lookup(target->d_parent, target->d_name))
                return -EEXIST;

        struct inode *inode = kmalloc(sizeof(*inode));
        if (!inode)
                return -ENOMEM;
        memset(inode, 0, sizeof(*inode));
        inode->i_mode = FT_DIRECTORY << 11;
        inode->i_nlink = 2;
        inode->i_sb = dir->i_sb;
        inode->i_op = &rootfs_ops;
        inode->i_fop = &rootfs_fops;
        INIT_LIST_HEAD(&inode->i_active_node);

        target->d_inode = inode;
        target->d_type = FT_DIRECTORY;
        target->d_sb = dir->i_sb;
        return 0;
}

static int_32 rootfs_rmdir(struct inode *dir, struct dentry *target)
{
        if (!dir || !target || !target->d_inode ||
            target->d_type != FT_DIRECTORY)
                return -EINVAL;
        kfree(target->d_inode);
        target->d_inode = NULL;
        return 0;
}


static struct inode *rootfs_create_root_inode(struct super_block *sb)
{
        struct inode *root_inode = kmalloc(sizeof(struct inode));
        if (!root_inode) {
                return NULL;
        }
        memset(root_inode, 0, sizeof(*root_inode));
        root_inode->i_sb = sb;
        root_inode->i_mode = FT_DIRECTORY << 11;
        root_inode->i_nlink = 2;
        root_inode->i_num = 0;
        root_inode->i_op = &rootfs_ops;
        root_inode->i_fop = &rootfs_fops;
        INIT_LIST_HEAD(&root_inode->i_active_node);

        return root_inode;
}

static struct super_block *rootfs_mount(struct fs_type *fs,
                                 int flags,
                                 const struct vfs_mount_source *source,
                                 void *data)
{
        (void) fs;
        (void) flags;
        (void) source;
        (void) data;
        struct super_block *rootsb = kmalloc(sizeof(struct super_block));
        if (!rootsb)
                return NULL;

        memset(rootsb, 0, sizeof(*rootsb));

        rootsb->s_devno = 0;
        rootsb->s_dev = NULL;
        rootsb->s_magic = ROOTFS_MAGIC;
        rootsb->s_fs_info = NULL;
        INIT_LIST_HEAD(&rootsb->s_inodes);

        struct inode *root_inode = rootfs_create_root_inode(rootsb);
        if (!root_inode) {
                kfree(rootsb);
                return NULL;
        }

        rootsb->s_root = root_inode;
        return rootsb;
}

static struct fs_type rootfs_type = {.name = "rootfs", .mount = rootfs_mount};

static void rootfs_destroy_super(struct super_block *sb)
{
        if (!sb)
                return;
        kfree(sb->s_root);
        kfree(sb);
}


static struct dentry *dentry_alloc_root(struct inode *root_inode)
{
        struct dentry *d = kmalloc(sizeof(struct dentry));
        if (!d)
                return NULL;
        memset(d, 0, sizeof(*d));
        d->d_inode = root_inode;
        d->d_parent = d;
        d->d_name = "/";
        INIT_LIST_HEAD(&d->d_subdirs);
        INIT_LIST_HEAD(&d->d_child_node);
        d->d_type = FT_DIRECTORY;
        d->d_sb = root_inode->i_sb;
        return d;
}

int root_fs_init(void)
{
        int ret = register_fs(&rootfs_type);
        if (ret < 0)
                return ret;
        // set global root dentry
        struct super_block *sb = rootfs_mount(NULL, 0, NULL, NULL);
        if (!sb) {
                unregister_fs(&rootfs_type);
                return -ENOMEM;
        }
        struct mount_entry *mentry = kmalloc(sizeof(struct mount_entry));
        if (!mentry) {
                rootfs_destroy_super(sb);
                unregister_fs(&rootfs_type);
                return -ENOMEM;
        }
        memset(mentry, 0, sizeof(*mentry));
        global_root_dentry = dentry_alloc_root(sb->s_root);
        if (!global_root_dentry) {
                kfree(mentry);
                rootfs_destroy_super(sb);
                unregister_fs(&rootfs_type);
                return -ENOMEM;
        }
        sb->s_mountpoint = global_root_dentry;
        mentry->fs_name = rootfs_type.name;
        mentry->sb = sb;
        mentry->mount_point = global_root_dentry;
        mentry->mounted_root = global_root_dentry;
        INIT_LIST_HEAD(&mentry->mount_node);
        add_mount_list(&mentry->mount_node);
        return 0;
}
