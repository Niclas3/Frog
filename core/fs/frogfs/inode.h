#ifndef __FS_INODE_H
#define __FS_INODE_H
#include <frog/types.h>
#include <frog/list.h>
#include <kernel/vfs.h>

#define ZONE_IDX_MAX 15

#define FROGFS_FIRST_IND_TLB_NO 11    // [11, 266]
#define FROGFS_SECOND_IND_TLB_NO 267  // [267, 522]
#define FROGFS_THIRD_IND_TLB_NO 523   // [523, 778]
#define FROGFS_FOURTH_IND_TLB_NO 779  // [779, 1034]
#define FROGFS_LAST_IND_TLB_NO 1034

/*
 * inode
 */

#define GET_FILE_TYPE(mode) ((mode) >> 11)

struct frogfs_inode {
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
    uint_32 i_size;           // file length in (bytes)
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
    uint_32 i_zones[ZONE_IDX_MAX];      // start address in lba
    uint_32 i_blocks;         // used block number
    /**************************************************************************/
    uint_32 i_atime;          // last access time
    uint_32 i_ctime;          // inode self state modified time 
    uint_32 i_mtime;          // modified time (from 1970.1.1:00:00:00, seconds)
    uint_16 i_uid;            // file owner's user id
    uint_8  i_gid;            // file owner's group id
    uint_16 i_dev;            // device number of inode
};

#define MAX_SINGLE_INODE_DATA_SIZE 140 // in ZONE_SIZE

struct inode *geti(struct super_block *sb, uint_32 inode_nr);
void new_inode(uint_32 inode_nr, struct inode* new_inode);

int flush_inode(struct super_block *sb, struct inode *inode, void *io_buf);
int clear_inode(struct super_block *sb, uint_32 inode_nr, void *io_buf);

#endif
