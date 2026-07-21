#ifndef _UAPI_FROG_FB_H
#define _UAPI_FROG_FB_H

#include <frog/types.h>

#define FROG_FB_IOCTL_GET_INFO 0x4601U

struct frog_fb_info {
        uint_32 width;
        uint_32 height;
        uint_32 pitch;
        uint_32 bits_per_pixel;
        uint_32 red_position;
        uint_32 red_size;
        uint_32 green_position;
        uint_32 green_size;
        uint_32 blue_position;
        uint_32 blue_size;
        uint_32 visible_length;
        uint_32 map_length;
};

#endif
