#ifndef __FROG_KERNEL_MOUNT_H
#define __FROG_KERNEL_MOUNT_H

#include <frog/list.h>

struct block_device;

enum vfs_mount_source_type {
        VFS_MOUNT_SOURCE_NONE = 0,
        VFS_MOUNT_SOURCE_PATH,
        VFS_MOUNT_SOURCE_BLOCK,
};

struct vfs_mount_source {
        enum vfs_mount_source_type type;
        union {
                const char *path;
                struct block_device *bdev;
        } value;
};

struct fs_type {
        const char *name;
        struct super_block *(*mount)(struct fs_type *fs,
                                     int flags,
                                     const struct vfs_mount_source *source,
                                     void *data);
        struct list_head fs_type_node;
};

struct mount_entry {
        const char *fs_name;
        struct dentry *mount_point;
        struct dentry *mounted_root;
        struct super_block *sb;
        struct list_head mount_node;
};

void init_mount_list(void);
void init_fs_type_list(void);

void add_mount_list(struct list_head *node);
struct dentry *find_mounted_dentry(struct dentry *dir);
int register_fs(struct fs_type *fs_type);
int unregister_fs(struct fs_type *fs_type);
struct fs_type *find_fs_type(const char *name);
struct mount_entry *find_mount_entry(const char *name);

#endif
