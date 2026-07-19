#ifndef __FROG_KERNEL_FRAMEBUFFER_H
#define __FROG_KERNEL_FRAMEBUFFER_H

#include <frog/types.h>

struct bus_type;
struct device;
struct phys_resource;

struct framebuffer_info {
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

struct framebuffer_ref {
        struct device *device;
        struct phys_resource *resource;
        struct framebuffer_info info;
};

int pc_framebuffer_snapshot_handoff(void);
int pc_framebuffer_register_aperture(struct bus_type *bus);
int pc_framebuffer_get_live(struct framebuffer_ref *ref);
void pc_framebuffer_put(struct framebuffer_ref *ref);

#ifdef CONFIG_QEMU_TEST
int pc_framebuffer_regression_test(void);
#endif

#endif
