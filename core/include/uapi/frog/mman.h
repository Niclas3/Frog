#ifndef _UAPI_FROG_MMAN_H
#define _UAPI_FROG_MMAN_H

#include <frog/types.h>

#define PROT_NONE       0x00U
#define PROT_READ       0x01U
#define PROT_WRITE      0x02U
#define PROT_EXEC       0x04U

#define MAP_SHARED      0x01U
#define MAP_PRIVATE     0x02U
#define MAP_FIXED       0x10U
#define MAP_ANONYMOUS   0x20U
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_FAILED      ((void *) -1)

struct frog_mmap_args {
        uint_32 addr;
        uint_32 length;
        uint_32 prot;
        uint_32 flags;
        int_32 fd;
        uint_32 offset;
};

#endif
