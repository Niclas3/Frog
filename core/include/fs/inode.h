#ifndef __FS_INODE_H
#define __FS_INODE_H

#include <ostype.h>
#include <list.h>

#define MAX_SINGLE_INODE_DATA_SIZE 140 // in ZONE_SIZE
struct partition;

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
    uint_16 i_count;          // linked count of inode, 0 presents free
    uint_8  i_lock;           // inode lock mark for write lock
    uint_8  i_dirt;           // inode dirty mark
    uint_8  i_pipe;           // inode is pipe 
    uint_8  i_mount;          // inode mount other file system
    uint_8  i_seek;           // search mark (when lseek() used)
    uint_8  i_update;         // inode is updated mark
    struct list_head inode_tag;
};

struct inode *inode_open(struct partition *part, uint_32 inode_nr);
void inode_close(struct inode *inode);
void new_inode(uint_32 inode_nr, struct inode* new_inode);
void flush_inode(struct partition *part,
                        struct inode *inode,
                        void *io_buf);

#endif
