#ifndef _FROG_FS
#define _FROG_FS
#include <frog/types.h>
#include <frog/list.h>
// #include <frog/fcntl.h>

struct dir;
struct file;
struct poll_table_struct;
struct partition;

#define stdin  0
#define stdout 1

enum file_type {
    FT_UNKOWN = 0,
    FT_FIFO = 1,
    FT_CHAR,
    FT_DIRECTORY,
    FT_BLOCK,
    FT_REGULAR,
};

#define IS_FT_CHAR(inode) (((inode)->i_mode >> 11)==FT_CHAR)
#define IS_FT_FIFO(inode) (((inode)->i_mode >> 11)==FT_FIFO)
#define IS_FT_DIRECTORY(inode) (((inode)->i_mode >> 11)==FT_DIRECTORY)
#define IS_FT_REGULAR(inode) (((inode)->i_mode >> 11)==FT_REGULAR)
#define validate(c)

enum exec_mode {
    EM_SET_USER_ID,
    EM_SET_GROUP_ID,
    EM_DIR_MAKR  // for directory rm limit mark
};

enum whence { SEEK_SET = 1, SEEK_CUR, SEEK_END };

// Copy from linux
struct stat {
    // dev_t st_dev;         /* ID of device containing file */
    int_32 st_ino;         /* Inode number */
    int_16 st_mode;       /* File type and mode */
    int_8  st_nlink;     /* Number of hard links */
    // uid_t st_uid;         /* User ID of owner */
    // gid_t st_gid;         /* Group ID of owner */
    // dev_t st_rdev;        /* Device ID (if special file) */
    int_32 st_size;        /* Total size, in bytes */
    // blksize_t st_blksize; /* Block size for filesystem I/O */
    int_32 st_zones;   /* Number of 512B blocks allocated */

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

// sector size is 512 bytes
#define SECTOR_SIZE 512
#define ZONE_SIZE SECTOR_SIZE
#define SECTOR_PER_ZONE 1  // ZONE_SIZE / SECTOR_SIZE
#define MAX_ZONE_COUNT 140
#define MAX_FILE_SIZE ZONE_SIZE *MAX_ZONE_COUNT
#define EOF(file) (file)->fd_inode->i_size + 1

/** file
 */
/*  Max file opening TIMES in system (one file can be reopen) */
#define MAX_FILE_OPEN       128

struct file{
    // offset of this file 
    uint_32 fd_pos; // next available byte
    uint_32 fd_flag;
    struct inode* fd_inode;
};

enum std_fd{
    FD_STDIN_NO,  // 0
    FD_STDOUT_NO, // 1
    FD_STDERR_NO  // 2
}; enum bitmap_type {
    INODE_BITMAP,
    ZONE_BITMAP
};

int_32 occupy_file_table_slot(void);
uint_32 fd_local2global(uint_32 local_fd);
struct file *get_file(uint_32 local_fd);
int_32 install_thread_fd(int_32 g_fd_idx);
int_32 inode_bitmap_alloc(struct partition *part);
uint_32 zone_bitmap_alloc(struct partition *part);
void flush_bitmap(struct partition *part,
                  enum bitmap_type b_type,
                  int_32 bit_idx);
int_32 file_create(struct partition *part,
                   struct dir *parent_d,
                   char *name,
                   uint_32 flag);
int_32 file_open(struct partition *part, uint_32 inode_nr, uint_8 flags);
int_32 file_close(struct file *file);
int_32 file_write(struct partition *part,
                  struct file *file,
                  const void *buf,
                  uint_32 write_len);
int_32 file_read(struct partition *part,
                 struct file *file,
                 void *buf,
                 uint_32 count);

/*
 * inode
 */
struct inode {
    uint_32 i_num;            // inode number
    /*
     * i_mode
     * +15+14+13+12+11+10+09+8+7-6+-----+----0+
     * |  |  |  |  |  |  |  |R|W|X|R|W|X|R|W|X|
     * +--+--+--+--+--+--+--+-+---+-----+-----+
     * \__________/ \_______/
     *       +          +
     *   file type    exec_mode
     * */
    uint_16 i_mode;           // file type and attributes (rwx bits)
    /****************************************************************/
    uint_16 i_uid;            // file owner's user id
    uint_32 i_size;           // file length in (bytes)
    uint_32 i_mtime;          // modified time (from 1970.1.1:00:00:00, seconds)
    uint_8  i_gid;            // file owner's group id
    uint_8  i_nlinks;         // links number. (how many directories link in this inode)
    /*
     * TODO: Maybe use 2-layer table to increasing one max file size 
     *  i_zones[0] - i_zones[11] all 12 zones for direct access
     *  i_zones[12] secondary access
     *  Each element in this array represents a address of zone (which size is
     *  512 bytes). 
     *  So i_zones[0-11] has 12 * ZONE_SIZE = 0x1800 bytes = 6144 bytes
     *  i_zones[12] contains a address to a direct table and which size is a
     *  ZONE_SIZE (aka 512 bytes). Each address size is 4 bytes, so our 
     *  1-layer table has (512bytes / 4 bytes = 128) 128 addresses, which has
     *  128 * ZONE_SIZE
     *  Over all we have (128 + 12 = 140) zones in one inode structure.
     * */
    uint_32 i_zones[13];      // start address in lba
    /**************************************************************************/
    uint_32 i_atime;          // last access time
    uint_32 i_ctime;          // inode self modified time
    uint_16 i_dev;            // device number of inode
    uint_16 i_count;          // open count of inode, 0 presents no one open it
    uint_8  i_lock;           // inode lock mark for write lock
    uint_8  i_dirt;           // inode dirty mark
    uint_8  i_pipe;           // inode is pipe 
    uint_8  i_mount;          // inode mount other file system
    uint_8  i_seek;           // search mark (when lseek() used)
    uint_8  i_update;         // inode is updated mark
    struct list_head inode_tag;
};

#define MAX_SINGLE_INODE_DATA_SIZE 140 // in ZONE_SIZE

struct inode *inode_open(struct partition *part, uint_32 inode_nr);
void inode_close(struct inode *inode);
void new_inode(uint_32 inode_nr, struct inode* new_inode);
void flush_inode(struct partition *part,
                        struct inode *inode,
                        void *io_buf);
void inode_release(struct partition *part, uint_32 inode_nr);

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

/*
 * super block 
 * TODO:!need do something
 **/
struct super_block {
    uint_32 s_magic;          // magic number of this file system 0x2023B07A
    uint_32 s_ninodes;        // inodes number
    uint_32 s_nzones;          // logic blocks number

    uint_32 s_imap_lba;       // inode bitmap start sector
    uint_32 s_imap_sz;        // inode bitmap size count in blocks count in sector (aka 512B)

    uint_32 s_zmap_lba;       // zone bitmap start sector
    uint_32 s_zmap_sz;        // zone (logic blocks) bitmap size count in blocks count in sector (aka 512B)

    uint_32 s_inode_table_lba; //inode table start sector
    uint_32 s_inode_table_sz;  // inode table size in sector
                                   //  512B       /    4kB
    uint_32 s_data_start_lba;
    uint_32 root_inode_no;
    uint_32 dir_entry_size;

    uint_32 s_log_zone_sz;    // log2(disk blocks / logic blocks)
    uint_32 s_max_file_sz;      // max length for one file

    uint_8 s_lock;             // locking mark
    uint_16 s_dev;             // device number of super block in
    uint_32 s_time;            // modified date
    uint_8 s_rd_only;          // read only mark
                               
    uint_8 s_dirt;             // dirty mark

    uint_8 pad[447];            // for up to 512 bytes
}__attribute__((packed));

/* 
 * dir
 *
 **/
#define MAX_FILE_NAME_LEN 16

extern struct dir root_dir;  // global variable for root directory

struct dir {
    struct inode *inode;
    uint_32 dir_pos;      // store current directory when use read_dir
    uint_8 dir_buf[512];  // dir_entry store current cursor dir entry 
};

struct dir_entry {
    char filename[MAX_FILE_NAME_LEN];
    uint_32 i_no;
    enum file_type f_type;
};

void open_root_dir(struct partition *part);
int_32 search_dir_entry(struct partition *part,
                        char *name,
                        struct dir *d,
                        struct dir_entry *entry);

struct dir *dir_open(struct partition *part, uint_32 inode_nr);
void dir_close(struct dir *d);

void new_dir_entry(char *name,
                   uint_32 inode_nr,
                   enum file_type file_type,
                   struct dir_entry *entry);

int_32 flush_dir_entry(struct partition *part,
                       struct dir *p_dir,
                       struct dir_entry *new_entry,
                       void *io_buf);

void delete_dir_entry(struct partition *part,
                      struct dir *pdir,
                      uint_32 inode_nr,
                      void *io_buf);
struct dir_entry *read_dir(struct dir *dirp);

bool dir_is_empty(struct dir *dirp);
int_32 dir_remove(struct partition *part,
                  struct dir *parent_dir,
                  struct dir *child_dir);

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
struct dir *sys_opendir(const char *name);
int_32 sys_closedir(struct dir *dirp);
struct dir_entry *sys_readdir(struct dir *dirp);
void sys_rewinddir(struct dir *dirp);
int_32 sys_rmdir(const char *pathname);
char *sys_getcwd(char *buf, int_32 size);
int_32 sys_chdir(const char *pathname);
int_32 sys_stat(const char *pathname, struct stat *statbuf);

int_32 sys_mount_device(const char *pathname, uint_32 dev_no, void *file);

uint_32 sys_poll(struct file *file, struct poll_table_struct *wait);
uint_32 sys_ioctl(int_32 fd, uint_32 request, void* argp);
#endif
