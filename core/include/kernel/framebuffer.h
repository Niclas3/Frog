#ifndef __FROG_KERNEL_FRAMEBUFFER_H
#define __FROG_KERNEL_FRAMEBUFFER_H

#include <frog/fb.h>
#include <frog/types.h>

struct bus_type;
struct device;
struct phys_resource;

struct framebuffer_ref {
        struct device *device;
        struct phys_resource *resource;
        struct frog_fb_info info;
};

int pc_framebuffer_snapshot_handoff(void);
int pc_framebuffer_register_aperture(struct bus_type *bus);
int pc_framebuffer_register_chardev(void);
int pc_framebuffer_mode_change_allowed(void);
int pc_framebuffer_unregister(void);
int pc_framebuffer_get_live(struct framebuffer_ref *ref);
void pc_framebuffer_put(struct framebuffer_ref *ref);

#ifdef CONFIG_QEMU_TEST
int pc_framebuffer_regression_test(void);
int pc_framebuffer_driver_regression_test(void);
#endif

#endif
