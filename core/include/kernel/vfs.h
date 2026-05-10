#ifndef __FROG_KERNEL_VFS_H
#define __FROG_KERNEL_VFS_H

#include <frog/block.h>
#include <frog/fcntl.h>
#include <frog/list.h>
#include <frog/types.h>
#include <kernel/mount.h>
#include <kernel/vfs_ops.h>

#define FILE_NAME_MAX 255

enum file_type {
        FT_UNKOWN = 0,
        FT_FIFO = 1,
        FT_CHAR,
        FT_DIRECTORY,
        FT_BLOCK,
        FT_REGULAR,
};

enum whence { SEEK_SET = 1, SEEK_CUR, SEEK_END };


struct super_block {
        uint_32 s_devno;              // dev number
        uint_32 s_block_size;         // logic block size (512)
        uint_32 s_magic;              // super block magic number
        uint_32 s_mount_time;
        struct dentry *s_mountpoint;  // mount path
        struct list_head s_inodes;    // active inode lists
        struct inode *s_root;
        struct super_operations *s_op;
        struct device *s_dev;
        struct block_device *s_bdev;
        void *s_fs_info;
};

struct inode {
        uint_32 i_num;  // inode number
        /*
         * i_mode
         * +15+14+13+12+11+10+09+8+7-6+-----+----0+
         * |  |  |  |  |  |  |  |R|W|X|R|W|X|R|W|X|
         * +--+--+--+--+--+--+--+-+---+-----+-----+
         * \__________/ \_______/
         *       +          +
         *   file type    exec_mode
         * */
        uint_16 i_mode;  // file type and attributes (rwx bits)
        uint_32 i_size;
        uint_32 i_nlink;
        uint_32 i_uid, i_gid;  // user infomations
        uint_32 i_atime, i_ctime, i_mtime;
        uint_32 i_dev;                   // for char device and block device
        struct list_head i_active_node;  // this a list node add to super block
                                         // active inode lists

        uint_8 i_lock;    // inode lock mark for write lock
        uint_16 i_count;  // open count of inode, 0 presents no one open it
        uint_8 i_dirty;   // inode dirty mark
        uint_8 i_mount;   // inode mount other file system
        uint_8 i_seek;    // search mark (when lseek() used)
        uint_8 i_update;  // inode is updated mark
        struct super_block *i_sb;
        struct inode_operations *i_op;
        struct file_operations *i_fop;
        struct block_device_operations *i_bdop;
        void *i_private;  // for inner real file system
};

struct file {
        // offset of this file
        uint_32 f_pos;  // next available byte
        uint_32 f_flag;
        struct inode *f_inode;
        struct file_operations *f_op;
        struct dentry *f_dentry;  //  related dir entry
        void *private_data;       //
};


struct dentry {
        char *d_name;
        struct inode *d_inode;       // target inode
        struct dentry *d_parent;     // parent dir entry
        struct list_head d_subdirs;  // mount tree of sub dirs
        struct list_head
            d_child_node;  // a list node to add parent->d_subdirs list
        uint_8 d_mounted;  // is mount point or not
        struct super_block *d_sb;
        void *d_fsdata;
        enum file_type d_type;
        uint_32 d_flags;
};



// vfs helper functions
struct dentry *dentry_lookup(struct dentry *parent, char *name);
void dentry_add_child(struct dentry *parent, struct dentry *child);
char *path_pop_tail(char *path, char *last_name);
char **next_path_components(char **p_path, char *component);

// Init some vfs infrastructure like mount list, fs type list.
int_32 vfs_init(void);

struct dentry *vfs_lookup(const char *path);

int_32 vfs_mount(const char *pathname,
                 const char *fs_type,
                 int flags,
                 const char *dev_name,
                 void *data);

struct file *vfs_open(char *path, uint_8 flags);
int_32 vfs_write(struct file *file, const void *buf, uint_32 count);
int_32 vfs_read(struct file *file, void *buf, uint_32 count);
int_32 vfs_lseek(struct file *file, int_32 offset, uint_8 whence);
uint_32 vfs_poll(struct file *file, struct poll_table_struct *wait);
uint_32 vfs_ioctl(struct file *file, uint_32 request, void *argp);
int_32 vfs_close(struct file *file);

int_32 vfs_mkdir(struct dentry *parent, struct dentry *child);
int_32 vfs_unlink(struct dentry *dir);
int_32 vfs_rmdir(struct dentry *dir);
int_32 vfs_readdir(struct file *dir, struct dentry *entry_out);
void vfs_rewinddir(struct file *dir);

// char *vfs_getcwd(char *buf, int_32 size);
// int_32 vfs_chdir(const char *pathname);
// int_32 vfs_stat(const char *pathname, struct stat *statbuf);


#endif
