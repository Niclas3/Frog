#include <frog/memory.h>
#include <frog/string.h>
#include <kernel/debug.h>
#include <kernel/vfs.h>

#define TMPFS_MAGIC 0xefeefeff

struct dentry *rootfs_lookup(struct inode *dir, struct dentry *target);
int_32 rootfs_mkdir(struct inode *dir, struct dentry *target, uint_32 mode);

static struct file_operations rootfs_fops = {0};
static struct inode_operations rootfs_ops = {.lookup = rootfs_lookup,
                                             .mkdir = rootfs_mkdir};

extern struct dentry *global_root_dentry;

// TODO: on process
// lookup in `layer_search`
struct dentry *rootfs_lookup(struct inode *dir, struct dentry *target)
{
        struct dentry *current = find_mount_entry("rootfs")->mount_point;

        return NULL;
}

int_32 rootfs_mkdir(struct inode *dir, struct dentry *target, uint_32 mode)
{
        return 0;
}


static struct inode *tmpfs_create_root_inode(struct super_block *sb)
{
        struct inode *root_inode = kmalloc(sizeof(struct inode));
        if (!root_inode) {
                return NULL;
        }
        root_inode->i_sb = sb;
        root_inode->i_mode = FT_DIRECTORY;
        root_inode->i_num = 0;
        root_inode->i_op = &rootfs_ops;
        root_inode->i_fop = &rootfs_fops;

        return root_inode;
}

static struct super_block *rootfs_mount(struct fs_type *fs,
                                 int flags,
                                 const char *dev,
                                 void *data)
{
        struct super_block *rootsb = kmalloc(sizeof(struct super_block));
        if (!rootsb)
                return NULL;

        // 1. use dev find devno and s_dev

        rootsb->s_devno = 0;
        rootsb->s_dev = NULL;
        rootsb->s_magic = TMPFS_MAGIC;
        rootsb->s_fs_info = NULL;
        INIT_LIST_HEAD(&rootsb->s_inodes);

        struct inode *root_inode = tmpfs_create_root_inode(rootsb);
        if (!root_inode)
                return NULL;

        rootsb->s_root = root_inode;
        return rootsb;
}

static struct fs_type rootfs_type = {.name = "rootfs", .mount = rootfs_mount};


static struct dentry *dentry_alloc_root(struct inode *root_inode)
{
        struct dentry *d = kmalloc(sizeof(struct dentry));
        d->d_inode = root_inode;
        d->d_parent = d;
        d->d_name = "/";
        INIT_LIST_HEAD(&d->d_subdirs);
        return d;
}

int root_fs_init(void)
{
        register_fs(&rootfs_type);
        // set global root dentry
        struct super_block *sb = rootfs_mount(NULL, 0, NULL, NULL);
        struct mount_entry *mentry = kmalloc(sizeof(struct mount_entry));
        global_root_dentry = dentry_alloc_root(sb->s_root);
        mentry->sb = sb;
        mentry->mount_point = global_root_dentry;
        add_mount_list(&mentry->mount_node);
        return 0;
}
