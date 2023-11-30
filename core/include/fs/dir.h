#ifndef __FS_DIR
#define __FS_DIR

#include <fs/fs.h>
#include <ostype.h>

#define MAX_FILE_NAME_LEN 16

struct partition;
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

int_32 dir_remove(struct partition *part,
                  struct dir *parent_dir,
                  struct dir *child_dir);
#endif
