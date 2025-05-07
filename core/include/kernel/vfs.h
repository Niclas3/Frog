#ifndef __FROG_KERNEL_VFS_H
#define __FROG_KERNEL_VFS_H

#include <frog/list.h>
#include <frog/types.h>
#include <kernel/vfs_ops.h>

enum file_type {
        FT_UNKOWN = 0,
        FT_FIFO = 1,
        FT_CHAR,
        FT_DIRECTORY,
        FT_BLOCK,
        FT_REGULAR,
};

struct super_block {
        uint_32 s_dev;                // dev number
        uint_32 s_block_size;         // logic block size (512)
        uint_32 s_magic;              // super block magic number
        struct dentry *s_mountpoint;  // mount path
        struct list_head s_inodes;    // active inode lists
        struct inode *s_root;
        struct super_operations *s_op;
        void *s_fs_info;
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
        struct list_head d_child_node;  // a list node to add parent->d_subdirs list
        uint_8 d_mounted;  // is mount point or not
        struct super_block *d_sb;
        void *d_fsdata;
        enum file_type d_type;
        uint_32 d_flags;
};


struct inode {
        uint_32 i_no;  // inode number
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
        uint_32 i_dev;                   // for char device
        struct list_head i_active_node;  // this a list node add to super block
                                         // active inode lists

        struct super_block *i_sb;
        struct inode_operations *i_op;
        struct file_operations *i_fop;
        void *i_private;  // for inner real file vfstem
};

// int_32 vfs_open(const char *pathname, uint_8 flags);
// int_32 vfs_close(int_32 fd);
// int_32 vfs_write(int_32 fd, const void *buf, uint_32 count);
// int_32 vfs_read(int_32 fd, void *buf, uint_32 count);
// int_32 vfs_lseek(int_32 fd, int_32 offset, uint_8 whence);
// int_32 vfs_unlink(const char *pathname);
// int_32 vfs_mkdir(const char *pathname);
// struct dir *vfs_opendir(const char *name);
// int_32 vfs_closedir(struct dir *dirp);
// struct dir_entry *vfs_readdir(struct dir *dirp);
// void vfs_rewinddir(struct dir *dirp);
// int_32 vfs_rmdir(const char *pathname);
// char *vfs_getcwd(char *buf, int_32 size);
// int_32 vfs_chdir(const char *pathname);
// int_32 vfs_stat(const char *pathname, struct stat *statbuf);
// int_32 vfs_mount_device(const char *pathname, uint_32 dev_no, void *file);
// uint_32 vfs_poll(struct file *file, struct poll_table_struct *wait);
// uint_32 vfs_ioctl(int_32 fd, uint_32 request, void* argp);

#endif
