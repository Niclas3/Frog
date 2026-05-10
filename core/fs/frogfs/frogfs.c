#include "frogfs.h"
#include <frog/block.h>
#include <frog/compiler.h>
#include <frog/errno.h>
#include <frog/memory.h>
#include <kernel/assert.h>
#include <kernel/debug.h>
#include <kernel/mount.h>
#include <kernel/panic.h>
#include <kernel/vfs.h>
#include "ffs_utils.h"

#include "inode.h"
#include "super_block.h"

void frogfs_put_super(struct super_block *sb);  // unmount timing
                                                //
int_32 frogfs_statfs(struct super_block *sb, struct stat *buf);
int_32 frogfs_remount(struct super_block *sb, uint_32 flags);

struct inode *frogfs_alloc_inode(struct super_block *sb);
void frogfs_destory_inode(struct super_block *sb, struct inode *target);

void frogfs_write_inode(struct inode *, struct writeback_control *);
void frogfs_evict_inode(struct inode *);
int frogfs_sync_fs(struct super_block *sb, int wait);

static struct super_block *frogfs_mount(struct fs_type *fs,
                                        int flags,
                                        const char *dev,
                                        void *data);

int_32 frogfs_open(struct inode *dinode, struct file *file);
int_32 frogfs_close(struct file *file);
int_32 frogfs_read(struct file *file, void *buf, uint_32 count);
int_32 frogfs_write(struct file *file, const void *buf, uint_32 count);
int_32 frogfs_lseek(struct file *file, int_32 offset, uint_8 whence);
int_32 frogfs_mmap(struct file *file, void *addr, uint_32 length, uint_32 flag);

int_32 frogfs_create(struct inode *dir, struct dentry *target, uint_32 mode);
int_32 frogfs_mkdir(struct inode *dir, struct dentry *target, uint_32 mode);
int_32 frogfs_rmdir(struct inode *dir, struct dentry *target);
int_32 frogfs_unlink(struct inode *dir, struct dentry *target);
struct dentry *frogfs_lookup(struct inode *dir, struct dentry *target);
int_32 frogfs_rename(struct inode *old_dir,
                     struct dentry *old_dentry,
                     struct inode *new_dir,
                     struct dentry *new_dentry);
int_32 frogfs_link(struct dentry *old_dentry,
                   struct inode *dir,
                   struct dentry *new_dentry);
int_32 frogfs_symlink(struct inode *dir,
                      struct dentry *target,
                      const char *symname);

static struct fs_type frogfs_type = {.name = "frogfs", .mount = frogfs_mount};
static struct super_operations frog_sop = {
    .put_super = frogfs_put_super,
    .statfs = frogfs_statfs,
    .remount = frogfs_remount,
    .alloc_inode = frogfs_alloc_inode,
    .destory_inode = frogfs_destory_inode,
    .write_inode = frogfs_write_inode,
    .sync_fs = frogfs_sync_fs,
    .evict_inode = frogfs_evict_inode,
};

static struct file_operations frog_fop = {.open = frogfs_open,
                                          .close = frogfs_close,
                                          .read = frogfs_read,
                                          .write = frogfs_write,
                                          .lseek = frogfs_lseek,
                                          .mmap = frogfs_mmap};

static struct inode_operations frog_iop = {
    .create = frogfs_create,
    .mkdir = frogfs_mkdir,
    .rmdir = frogfs_rmdir,
    .unlink = frogfs_unlink,
    .lookup = frogfs_lookup,
    .rename = frogfs_rename,
    .link = frogfs_link,
    .symlink = frogfs_symlink,
};


static struct dentry_operations frog_dop = {};

static struct address_space_operations frog_asop = {};


void frogfs_put_super(struct super_block *sb)
{
        return;
}

int_32 frogfs_statfs(struct super_block *sb, struct stat *buf)
{
        return 0;
}

int_32 frogfs_remount(struct super_block *sb, uint_32 flags)
{
        return 0;
}

struct inode *frogfs_alloc_inode(struct super_block *sb)
{
        int_32 inum = alloc_inode_bitmap(sb);
        if (inum == -1) {
                DEBUG("not enough inode number");
                return NULL;
        }

        struct inode *inode = kmalloc(sizeof(struct inode));
        struct frogfs_inode *finode = kmalloc(sizeof(struct frogfs_inode));
        finode->i_num = inum;
        if (!inode) {
                DEBUG("not enough memory");
                return NULL;
        }
        inode->i_num = inum;
        inode->i_op = sb->s_root->i_op;
        inode->i_fop = sb->s_root->i_fop;
        inode->i_private = finode;

        return inode;
}

void frogfs_destory_inode(struct super_block *sb, struct inode *target)
{
        // free all resource of inode
        // mark all zone in this inode as free
        ASSERT(target);
        if (!target) {
                return;
        }

        uint_32 inum = target->i_num;

        if (target->i_private) {
                kfree(target->i_private);
        }

        kfree(target);

        free_inode_bitmap(sb, inum);

        return;
}

void frogfs_write_inode(struct inode *inode, struct writeback_control *wbc)
{
        (void) inode;
        (void) wbc;
        return;
}

void frogfs_evict_inode(struct inode *inode)
{
        (void) inode;
        return;
}

int frogfs_sync_fs(struct super_block *sb, int wait)
{
        // flush zone bitmap
        // flush inode bitmap
        // flush all dirty inodes
        // flush all dirty dir blocks
        // flush super blocks
        return 0;
}

static struct inode *frogfs_create_root_inode(struct block_device *bdev,
                                              struct super_block *sb)
{
        struct inode *root_inode = kmalloc(sizeof(struct inode));
        if (!root_inode) {
                return NULL;
        }
        struct frogfs_super_block *fsb =
            (struct frogfs_super_block *) sb->s_fs_info;

        uint_8 *buf = kmalloc(fsb->disk_sb.s_inode_table_sz * ZONE_SIZE);

        // read root inode from disk
        int size = read_blocks(sb, fsb->disk_sb.s_inode_table_blk,
                               fsb->disk_sb.s_inode_table_sz, buf);
        if (!size) {
                return NULL;
        }
        struct frogfs_inode *inode_table = (struct frogfs_inode *) buf;
        uint_32 root_inode_no = ((struct frogfs_super_block *) sb->s_fs_info)
                                    ->disk_sb.root_inode_no;
        struct frogfs_inode *finode = &inode_table[root_inode_no];

        root_inode->i_private = finode;
        root_inode->i_op = &frog_iop;
        root_inode->i_fop = &frog_fop;
        root_inode->i_bdop = NULL;
        root_inode->i_sb = sb;
        root_inode->i_mode = finode->i_mode;
        root_inode->i_num = finode->i_num;
        root_inode->i_size = finode->i_size;

        kfree(buf);
        return root_inode;
}

static struct super_block *frogfs_mount(struct fs_type *fs,
                                        int flags,
                                        const char *dev,
                                        void *data)
{
        struct super_block *sb = kmalloc(sizeof(struct super_block));
        if (!sb)
                return NULL;

        // /dev/sdb0p1
        struct dentry *bdev = vfs_lookup(dev);
        ASSERT(bdev);
        dev_t bdev_no = bdev->d_inode->i_dev;
        struct block_device *frogfs_bdev = get_block_device(bdev_no);
        if (!frogfs_bdev) {
                INFO("[frogfs]: not find target block device %d", bdev_no);
                PANIC("!");
                return NULL;
        }

        struct frogfs_super_block *fsb =
            kmalloc(sizeof(struct frogfs_super_block));

        // read_super_block_from_disk fill into fsb
        int rd_size = bio_read(frogfs_bdev, frogfs_bdev->bd_start_lba, fsb, 1);
        if (rd_size <= 0) {
                INFO("[frogfs]: read super block error ");
        }
        if (fsb->disk_sb.s_magic != FROGFS_MAGIC) {
                INFO("[frogfs]: super block magic error");
                PANIC("!");
        }

        sb->s_fs_info = fsb;
        sb->s_devno = bdev_no;
        sb->s_magic = fsb->disk_sb.s_magic;
        sb->s_op = &frog_sop;
        sb->s_block_size = fsb->disk_sb.s_zone_sz;
        sb->s_bdev = frogfs_bdev;

        // read inode bitmap from disk
        struct bitmap *i_bmap = kmalloc(sizeof(struct bitmap));
        int ibmap_start = fsb->disk_sb.s_imap_blk;
        int ibmap_size = fsb->disk_sb.s_imap_sz;
        read_bitmap(sb, ibmap_start, ibmap_size, i_bmap);

        // read zone bitmap from disk
        struct bitmap *z_bmap = kmalloc(sizeof(struct bitmap));
        int zbmap_start = fsb->disk_sb.s_zmap_blk;
        int zbmap_size = fsb->disk_sb.s_zmap_sz;
        read_bitmap(sb, zbmap_start, zbmap_size, z_bmap);

        fsb->i_bmap = i_bmap;
        fsb->z_bmap = z_bmap;

        INIT_LIST_HEAD(&sb->s_inodes);

        struct inode *root_inode = frogfs_create_root_inode(frogfs_bdev, sb);

        list_add_tail(&root_inode->i_active_node, &sb->s_inodes);

        if (!root_inode)
                return NULL;

        sb->s_root = root_inode;
        return sb;
}


int_32 frogfs_open(struct inode *dinode, struct file *file)
{
        (void) dinode;
        (void) file;
        return 0;
}

int_32 frogfs_close(struct file *file)
{
        (void) file;
        return 0;
}

int_32 frogfs_read(struct file *file, void *buf, uint_32 count){
        (void) file;
        (void) count;
        ASSERT(buf);
        // count is in bytes



        return 0;
}
int_32 frogfs_write(struct file *file, const void *buf, uint_32 count){
        (void) file;
        (void) count;
        ASSERT(buf);

        return 0;
}

int_32 frogfs_lseek(struct file *file, int_32 offset, uint_8 whence)
{
        if (!file)
                return -EINVAL;

        switch (whence) {
        case SEEK_SET:
                file->f_pos = offset;
                break;
        case SEEK_CUR:
                file->f_pos += offset;
                break;
        case SEEK_END:
                file->f_pos = file->f_inode->i_size + offset;
                break;
        default:
                return -EINVAL;
        }
        return file->f_pos;
}

int_32 frogfs_create(struct inode *dir, struct dentry *target, uint_32 mode)
{
        (void) dir;
        (void) target;
        (void) mode;
        return -ENOSYS;
}

int_32 frogfs_mkdir(struct inode *dir, struct dentry *target, uint_32 mode)
{
        (void) dir;
        (void) target;
        (void) mode;
        return -ENOSYS;
}

int_32 frogfs_rmdir(struct inode *dir, struct dentry *target)
{
        (void) dir;
        (void) target;
        return -ENOSYS;
}

int_32 frogfs_unlink(struct inode *dir, struct dentry *target)
{
        (void) dir;
        (void) target;
        return -ENOSYS;
}

struct dentry *frogfs_lookup(struct inode *dir, struct dentry *target)
{
        (void) dir;
        (void) target;
        return NULL;
}

int_32 frogfs_rename(struct inode *old_dir,
                     struct dentry *old_dentry,
                     struct inode *new_dir,
                     struct dentry *new_dentry)
{
        (void) old_dir;
        (void) old_dentry;
        (void) new_dir;
        (void) new_dentry;
        return -ENOSYS;
}

int_32 frogfs_link(struct dentry *old_dentry,
                   struct inode *dir,
                   struct dentry *new_dentry)
{
        (void) old_dentry;
        (void) dir;
        (void) new_dentry;
        return -ENOSYS;
}

int_32 frogfs_symlink(struct inode *dir,
                      struct dentry *target,
                      const char *symname)
{
        (void) dir;
        (void) target;
        (void) symname;
        return -ENOSYS;
}

int_32 frogfs_mmap(struct file *file, void *addr, uint_32 length, uint_32 flag)
{
        (void) file;
        (void) addr;
        (void) length;
        (void) flag;
        return -ENOSYS;
}


extern struct dentry *global_root_dentry;
static int make_mount_point(struct dentry *root, char *path)
{
        struct dentry *child = kmalloc(sizeof(struct dentry));
        if (!child)
                return -1;
        child->d_name = "test";
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

int frogfs_init(void)
{
        register_fs(&frogfs_type);
        make_mount_point(global_root_dentry, "test");
        return 0;
}
