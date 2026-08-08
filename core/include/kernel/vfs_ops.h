#ifndef __FROG_KERNEL_VFS_OPS_H
#define __FROG_KERNEL_VFS_OPS_H
#include <frog/types.h>

struct file;
struct inode;
struct dentry;
struct super_block;
struct poll_table_struct;
struct stat;
struct page;
struct vm_area;


struct file_operations {
        int_32 (*open)(struct inode *inode, struct file *file);
        int_32 (*close)(struct file *file);
        int_32 (*read)(struct file *file, void *buf, uint_32 count);
        int_32 (*write)(struct file *file, const void *buf, uint_32 count);
        int_32 (*lseek)(struct file *file, int_32 offset, uint_8 whence);
        uint_32 (*poll)(struct file *file, struct poll_table_struct *wait);
        int_32 (*ioctl)(struct file *file, uint_32 request, void *argp);
        int_32 (*mmap)(struct file *file, struct vm_area *vma);
};

struct inode_operations {
        /*
         * Opens one dynamically named child atomically.  VFS supplies an
         * unlinked candidate dentry and an initialized, otherwise empty file.
         * Success must publish or select the dentry and fill file identity;
         * failure must leave the namespace unchanged.
         */
        int_32 (*atomic_open)(struct inode *dir,
                              struct dentry *candidate,
                              struct file *file,
                              uint_32 flags);
        int_32 (*create)(struct inode *dir,
                         struct dentry *target,
                         uint_32 mode);
        int_32 (*mkdir)(struct inode *dir, struct dentry *target, uint_32 mode);
        int_32 (*rmdir)(struct inode *dir, struct dentry *target);
        int_32 (*unlink)(struct inode *dir, struct dentry *target);
        struct dentry *(*lookup)(struct inode *dir, struct dentry *target);
        int_32 (*rename)(struct inode *old_dir,
                         struct dentry *old_dentry,
                         struct inode *new_dir,
                         struct dentry *new_dentry);
        int_32 (*link)(struct dentry *old_dentry,
                       struct inode *dir,
                       struct dentry *new_dentry);
        int_32 (*symlink)(struct inode *dir,
                          struct dentry *target,
                          const char *symname);
};

struct writeback_control {

};

struct super_operations {
        void (*put_super)(struct super_block *sb);
        int_32 (*statfs)(struct super_block *sb, struct stat *buf);
        int_32 (*remount)(struct super_block *sb, uint_32 flags);
        struct inode *(*alloc_inode)(struct super_block *sb);
        void (*destory_inode)(struct super_block *sb, struct inode *target);

        void (*write_inode)(struct inode *, struct writeback_control *);
        void (*evict_inode)(struct inode *);
        int  (*sync_fs)(struct super_block *sb, int wait);
};

struct dentry_operations {
        int_32 (*d_compare)(struct dentry *dentry,
                            const char *name,
                            uint_32 len);
        int_32 (*d_revalidate)(struct dentry *dentry,
                               uint_32 flags);  // cache related
};

// TODO: this structure  for  `mmap()` 
struct address_space_operations {
    int_32 (*readpage)(struct file *file, struct page *page);
    int_32 (*writepage)(struct page *page);
    int_32 (*write_begin)(struct file *file, struct page *page,
                          uint_32 offset, uint_32 length);
    int_32 (*write_end)(struct file *file, struct page *page,
                        uint_32 offset, uint_32 length, const void *buf);
};

#endif
