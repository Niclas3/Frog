#ifndef __FS_FILE_H
#define __FS_FILE_H
#include <ostype.h>
struct partition;
struct dir;
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
};

enum bitmap_type {
    INODE_BITMAP,
    ZONE_BITMAP
};

#define MAX_FILE_OPEN 128   //Max file opening TIMES in system (one file can be
                           //reopen )

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
#endif
