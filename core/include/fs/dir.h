#ifndef __FS_DIR
#define __FS_DIR

#include <ostype.h>

#define MAX_FILE_NAME_LEN 16

struct dir {
    struct inode *inode;
    uint_32 dir_pos;                
    uint_8  dir_buf[512];
};

struct dir_entry{
    char filename[MAX_FILE_NAME_LEN];
    uint_32 i_no;
};

#endif
