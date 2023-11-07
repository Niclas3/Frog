#ifndef __FS_INODE_H
#define __FS_INODE_H

#include <ostype.h>
#include <list.h>

struct inode {
    uint_16 i_num;            // inode number
    uint_16 i_mode;           // file type and attributes (rwx bits)
    uint_16 i_uid;            // file owner's user id
    uint_32 i_size;           // file length in (bytes)
    uint_32 i_mtime;          // modified time (from 1970.1.1:00:00:00, seconds)
    uint_8  i_gid;            // file owner's group id
    uint_8  i_nlinks;         // links number. (how many directories link in this inode)
    uint_16 i_zones[13];      //
    uint_32 i_atime;          // last access time
    uint_32 i_ctime;          // inode self modified time
    uint_16 i_dev;            // device number of inode
    uint_16 i_count;          // linked count of inode, 0 presents free
    uint_8  i_lock;           // inode lock mark for write lock
    uint_8  i_dirt;           // inode dirty mark
    uint_8  i_pipe;           // inode is pipe 
    uint_8  i_mount;          // inode mount other file system
    uint_8  i_seek;           // search mark (when lseek() used)
    uint_8  i_update;         // inode is updated mark
    struct list_head inode_tag;
};

#endif
