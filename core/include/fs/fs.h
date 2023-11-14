#ifndef __FS_FS
#define __FS_FS

#define MAX_FILES_PER_PARTITION 4096


// 512 bytes * 8 bits/bytes = 4096 bits
#define BITS_PER_SECTOR 4096

//sector size is 512 bytes
#define SECTOR_SIZE 512
#define ZONE_SIZE SECTOR_SIZE
struct disk;

enum file_type{
    FT_UNKOWN=0,
    FT_FIFO=1,
    FT_CHAR,
    FT_DIRECTORY,
    FT_BLOCK,
    FT_REGULAR,
};

enum exec_mode{
    EM_SET_USER_ID,
    EM_SET_GROUP_ID,
    EM_DIR_MAKR     // for directory rm limit mark
};

void fs_init(void);
#endif
