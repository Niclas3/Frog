#ifndef __FS_DIR
#define __FS_DIR

#include <ostype.h>

#define MAX_FILE_NAME_LEN 16

struct partition;

struct dir {
    struct inode *inode;
    uint_32 dir_pos;
    uint_8 dir_buf[512];  // for dir_entry in this dir
};

struct dir_entry {
    char filename[MAX_FILE_NAME_LEN];
    uint_32 i_no;
};

void open_root_dir(struct partition *part);
int_32 search_dir_entry(struct partition *part,
                        char *name,
                        struct dir *d,
                        struct dir_entry *entry);

struct dir *dir_open(struct partition *part, uint_32 inode_nr);
void dir_close(struct dir *d);
void new_dir_entry(char *name, uint_32 inode_nr, struct dir_entry *entry);
#endif
