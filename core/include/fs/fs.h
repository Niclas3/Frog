#ifndef __FS_FS
#define __FS_FS

#define MAX_FILES_PER_PARTITION 4096


// 512 bytes * 8 bits/bytes = 4096 bits
#define BITS_PER_SECTOR 4096

//sector size is 512 bytes
#define SECTOR_SIZE 512
#define ZONE_SIZE SECTOR_SIZE

enum file_type{
    FT_UNKOWN,
    FT_REGULAR,        
    FT_DIRECTORY,
};
#endif
