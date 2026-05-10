#ifndef __FS_DIR
#define __FS_DIR
#include <frog/types.h>
#include "super_block.h"
#include <frog/list.h>

/* 
 * dir
 *
 **/

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
#define MAX_FILE_NAME_LEN 16

// memory data struct 
struct dentry {
        char *d_name;
        struct inode *d_inode;       // target inode
        struct dentry *d_parent;     // parent dir entry
        struct list_head d_subdirs;  // mount tree of sub dirs
        struct list_head
            d_child_node;  // a list node to add parent->d_subdirs list
        uint_8 d_mounted;  // is mount point or not
        struct super_block *d_sb;
        void *d_fsdata;
        enum file_type d_type;
        uint_32 d_flags;
};

// a data struct at disk side
struct dir_entry {
    uint_32 i_no;
    enum file_type f_type;
    uint_16 entry_len; // lenght of this entry
    uint_8 name_len;   // name 
    char filename[];  // not null end
};


int_32 search_dir_entry(struct super_block *sb,
                        char *name,
                        struct dentry *d,
                        struct dir_entry *entry);

struct dentry *dir_open(struct super_block *sb, uint_32 inode_nr);
void dir_close(struct dentry *d);

void new_dir_entry(char *name,
                   uint_32 inode_nr,
                   enum file_type file_type,
                   struct dir_entry *entry);

int_32 flush_dir_entry(struct super_block *sb,
                       struct dentry *p_dir,
                       struct dir_entry *new_entry,
                       void *io_buf);

void delete_dir_entry(struct super_block *sb,
                      struct dentry *pdir,
                      uint_32 inode_nr,
                      void *io_buf);

struct dir_entry *read_dir(struct dentry *dirp);

bool dir_is_empty(struct dentry *dirp);
int_32 dir_remove(struct super_block *sb,
                  struct dentry *parent_dir,
                  struct dentry *child_dir);

#endif
