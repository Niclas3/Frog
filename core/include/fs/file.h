#ifndef __FS_FILE_H
#define __FS_FILE_H
#include <ostype.h>
struct partition;
struct file{
    // offset of this file 
    uint_32 fd_pos;
    uint_32 fd_flag;
    struct inode* fd_inode;
};

enum std_fd{
    stdin_no,  // 0
    stdout_no, // 1
    stderr_no  // 2
};

enum bitmap_type {
    INODE_BITMAP,
    ZONE_BITMAP
};

#define MAX_FILE_OPEN 32   //Max file opening TIMES in system (one file can be
                           //reopen )

int_32 occupy_file_table_slot(void);
int_32 install_thread_fd(int_32 g_fd_idx);
int_32 inode_bitmap_alloc(struct partition *part);
uint_32 zone_bitmap_alloc(struct partition *part);
void flush_bitmap(struct partition *part,
                  enum bitmap_type b_type,
                  int_32 bit_idx);
#endif
