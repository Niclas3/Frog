#ifndef _FROGFS_H
#define _FROGFS_H

#include <frog/types.h>
#include "super_block.h"

struct dentry;
struct file;
struct inode;
struct writeback_control;


struct stat {
        int_32 st_ino;
        int_16 st_mode;
        int_8 st_nlink;
        int_32 st_size;
        int_32 st_zones;
};

#define MAX_FILES_PER_PARTITION 4096

#define BITS_PER_SECTOR 4096
#define BITS_PER_ZONE 8192

#define SECTOR_SIZE 512
#define ZONE_SIZE (SECTOR_SIZE << 1)
#define SECTOR_PER_ZONE 2
#define MAX_ZONE_COUNT (11 + ((ZONE_SIZE / 4 * 4)))
#define MAX_FILE_SIZE (MAX_ZONE_COUNT * ZONE_SIZE)
#define EOF(file) (file)->f_inode->i_size + 1

enum frogfs_bmap_t { INODE_BITMAP, ZONE_BITMAP };

void frogfs_put_super(struct super_block *sb);
int_32 frogfs_statfs(struct super_block *sb, struct stat *buf);
int_32 frogfs_remount(struct super_block *sb, uint_32 flags);
struct inode *frogfs_alloc_inode(struct super_block *sb);
void frogfs_destory_inode(struct super_block *sb, struct inode *target);
void frogfs_write_inode(struct inode *inode, struct writeback_control *wbc);
void frogfs_evict_inode(struct inode *inode);
int frogfs_sync_fs(struct super_block *sb, int wait);

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

int frogfs_init(void);

#endif
