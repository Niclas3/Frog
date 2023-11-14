#ifndef __FS_FILE_H
#define __FS_FILE_H
#include <ostype.h>
struct file{
    uint_32 fd_pos;
    uint_32 fd_flag;
    struct inode* fd_inode;
};

enum std_fd{
    stdin_no,
    stdout_no,
    stderr_no
};

enum bitmap_type {
    INODE_BITMAP,
    ZONE_BITMAP
};

#define MAX_FILE_OPEN 32   //Max file opening TIMES in system (one file can be
                           //reopen )

#endif
