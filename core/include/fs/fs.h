#ifndef __FS_FS
#define __FS_FS
#include <fs/fcntl.h>
#include <ostype.h>

#define MAX_FILES_PER_PARTITION 4096


// 512 bytes * 8 bits/bytes = 4096 bits
#define BITS_PER_SECTOR 4096

// sector size is 512 bytes
#define SECTOR_SIZE 512
#define ZONE_SIZE SECTOR_SIZE
#define SECTOR_PER_ZONE 1  // ZONE_SIZE / SECTOR_SIZE
#define MAX_ZONE_COUNT 140
#define MAX_FILE_SIZE ZONE_SIZE *MAX_ZONE_COUNT
#define EOF(file) (file)->fd_inode->i_size + 1

struct dir;

extern struct partition mounted_part;  // the partition what we want to mount.

enum file_type {
    FT_UNKOWN = 0,
    FT_FIFO = 1,
    FT_CHAR,
    FT_DIRECTORY,
    FT_BLOCK,
    FT_REGULAR,
};

enum exec_mode {
    EM_SET_USER_ID,
    EM_SET_GROUP_ID,
    EM_DIR_MAKR  // for directory rm limit mark
};

enum whence { SEEK_SET = 1, SEEK_CUR, SEEK_END };

void fs_init(void);
int_32 path_depth(const char *path);

int_32 sys_open(const char *pathname, uint_8 flags);
int_32 sys_close(int_32 fd);
int_32 sys_write(int_32 fd, const void *buf, uint_32 count);
int_32 sys_read(int_32 fd, void *buf, uint_32 count);
int_32 sys_lseek(int_32 fd, int_32 offset, uint_8 whence);
int_32 sys_unlink(const char *pathname);
int_32 sys_mkdir(const char *pathname);
struct dir *sys_opendir(const char *name);
int_32 sys_closedir(struct dir *dirp);
struct dir_entry *sys_readdir(struct dir *dirp);
void sys_rewinddir(struct dir *dirp);
int_32 sys_rmdir(const char *pathname);
#endif
