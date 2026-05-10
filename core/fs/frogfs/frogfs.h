#ifndef _FROG_FS
#define _FROG_FS
#include <frog/list.h>
#include <frog/types.h>
// #include <frog/fcntl.h>
#include "super_block.h"

struct dir;
struct file;
struct poll_table_struct;
struct partition;

#define stdin 0
#define stdout 1


enum exec_mode {
        EM_SET_USER_ID,
        EM_SET_GROUP_ID,
        EM_DIR_MAKR  // for directory rm limit mark
};


// Copy from linux
struct stat {
        // dev_t st_dev;         /* ID of device containing file */
        int_32 st_ino;  /* Inode number */
        int_16 st_mode; /* File type and mode */
        int_8 st_nlink; /* Number of hard links */
        // uid_t st_uid;         /* User ID of owner */
        // gid_t st_gid;         /* Group ID of owner */
        // dev_t st_rdev;        /* Device ID (if special file) */
        int_32 st_size; /* Total size, in bytes */
        // blksize_t st_blksize; /* Block size for filesystem I/O */
        int_32 st_zones; /* Number of 512B blocks allocated */

        // struct timespec st_atim; /* Time of last access */
        // struct timespec st_mtim; /* Time of last modification */
        // struct timespec st_ctim; /* Time of last status change */

        // #define st_atime st_atim.tv_sec /* Backward compatibility */
        // #define st_mtime st_mtim.tv_sec
        // #define st_ctime st_ctim.tv_sec
};

extern struct partition mounted_part;  // the partition what we want to mount.

#define MAX_FILES_PER_PARTITION 4096

// 512 bytes * 8 bits/bytes = 4096 bits
#define BITS_PER_SECTOR 4096
#define BITS_PER_ZONE 8192

// sector size is 512 bytes
#define SECTOR_SIZE 512
#define ZONE_SIZE (SECTOR_SIZE << 1)
#define SECTOR_PER_ZONE 2  // ZONE_SIZE / SECTOR_SIZE
#define MAX_ZONE_COUNT (11  + ((ZONE_SIZE / 4 * 4)))
#define MAX_FILE_SIZE (MAX_ZONE_COUNT * ZONE_SIZE)  // almost 1.1MB
#define EOF(file) (file)->f_inode->i_size + 1
enum frogfs_bmap_t { INODE_BITMAP, ZONE_BITMAP };

/** file
 */
/*  Max file opening TIMES in system (one file can be reopen) */
// #define MAX_FILE_OPEN 128
//
// struct file {
//         // offset of this file
//         uint_32 fd_pos;  // next available byte
//         uint_32 fd_flag;
//         struct inode *fd_inode;
// };
//
// enum std_fd {
//         FD_STDIN_NO,   // 0
//         FD_STDOUT_NO,  // 1
//         FD_STDERR_NO   // 2
// };

// int_32 occupy_file_table_slot(void);
// uint_32 fd_local2global(uint_32 local_fd);
// struct file *get_file(uint_32 local_fd);
// int_32 install_thread_fd(int_32 g_fd_idx);
// int_32 inode_bitmap_alloc(struct partition *part);
// uint_32 zone_bitmap_alloc(struct partition *part);
// void flush_bitmap(struct partition *part,
//                   enum bitmap_type b_type,
//                   int_32 bit_idx);
// int_32 file_create(struct partition *part,
//                    struct dir *parent_d,
//                    char *name,
//                    uint_32 flag);
// int_32 file_open(struct partition *part, uint_32 inode_nr, uint_8 flags);
// int_32 file_close(struct file *file);
// int_32 file_write(struct partition *part,
//                   struct file *file,
//                   const void *buf,
//                   uint_32 write_len);
// int_32 file_read(struct partition *part,
//                  struct file *file,
//                  void *buf,
//                  uint_32 count);


/* pipe
 *
 * */

#define PIPE_FLAG 0xffffff

bool is_pipe(int_32 fd);
int_32 sys_pipe(int_32 pipefd[2]);

int_32 open_pipe(int_32 fd);
void close_pipe(int_32 fd);

uint_32 read_pipe(int_32 fd, void *buf, uint_32 count);
uint_32 write_pipe(int_32 fd, const void *buf, uint_32 count);
int_32 pipe_check(int_32 fd);


//////


void fs_init(void);
int_32 path_depth(const char *path);

int_32 sys_open(const char *pathname, uint_8 flags);
int_32 sys_close(int_32 fd);
int_32 sys_write(int_32 fd, const void *buf, uint_32 count);
int_32 sys_read(int_32 fd, void *buf, uint_32 count);
int_32 sys_lseek(int_32 fd, int_32 offset, uint_8 whence);
int_32 sys_unlink(const char *pathname);
int_32 sys_mkdir(const char *pathname);

struct dentry *sys_opendir(const char *name);
int_32 sys_closedir(struct dentry *dirp);
struct dir_entry *sys_readdir(struct dentry *dirp);

void sys_rewinddir(struct dentry *dirp);
int_32 sys_rmdir(const char *pathname);
char *sys_getcwd(char *buf, int_32 size);
int_32 sys_chdir(const char *pathname);
int_32 sys_stat(const char *pathname, struct stat *statbuf);

uint_32 sys_poll(struct file *file, struct poll_table_struct *wait);
uint_32 sys_ioctl(int_32 fd, uint_32 request, void *argp);
#endif
