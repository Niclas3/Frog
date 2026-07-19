#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/phys_resource.h>
#include <frog/types.h>
#include <kernel/framebuffer.h>
#include <kernel/framebuffer_smoke.h>
#include <kernel/qemu_test.h>

#define FRAMEBUFFER_TEST_WIDTH  1024U
#define FRAMEBUFFER_TEST_HEIGHT 768U

#ifdef CONFIG_FROG_TEST_FRAMEBUFFER
static uint_32 channel_mask(uint_32 width)
{
        return (1U << width) - 1U;
}

static uint_32 encode_channel(uint_8 value, uint_32 width, uint_32 position)
{
        return ((value >> (8U - width)) & channel_mask(width)) << position;
}

static uint_32 encode_rgb(const struct framebuffer_info *info,
                          uint_8 red, uint_8 green, uint_8 blue)
{
        return encode_channel(red, info->red_size, info->red_position) |
               encode_channel(green, info->green_size,
                              info->green_position) |
               encode_channel(blue, info->blue_size, info->blue_position);
}

void framebuffer_smoke_run(void)
{
        struct framebuffer_ref framebuffer;
        uint_8 *memory;
        uint_32 red;
        uint_32 green;
        uint_32 blue;
        uint_32 white;

        if (pc_framebuffer_get_live(&framebuffer) != 0)
                frog_test_abort("framebuffer-resource-unavailable");
        if (framebuffer.info.width != FRAMEBUFFER_TEST_WIDTH ||
            framebuffer.info.height != FRAMEBUFFER_TEST_HEIGHT ||
            framebuffer.resource->start > 0xffffffffULL ||
            !phys_resource_contains(framebuffer.resource,
                                    framebuffer.resource->start,
                                    framebuffer.info.map_length)) {
                pc_framebuffer_put(&framebuffer);
                frog_test_abort("framebuffer-resource-invalid");
        }
        if (map_kernel_framebuffer_pinned(framebuffer.resource,
                                          framebuffer.info.map_length) != 0) {
                pc_framebuffer_put(&framebuffer);
                frog_test_abort("framebuffer-map-failed");
        }

        memory = (uint_8 *) KERNEL_FRAMEBUFFER_VADDR;
        red = encode_rgb(&framebuffer.info, 255, 0, 0);
        green = encode_rgb(&framebuffer.info, 0, 255, 0);
        blue = encode_rgb(&framebuffer.info, 0, 0, 255);
        white = encode_rgb(&framebuffer.info, 255, 255, 255);

        for (uint_32 y = 0; y < framebuffer.info.height; y++) {
                uint_32 *row =
                    (uint_32 *) (memory + y * framebuffer.info.pitch);

                for (uint_32 x = 0; x < framebuffer.info.width; x++) {
                        uint_32 color =
                            x < framebuffer.info.width / 3U
                                ? red
                                : (x < 2U * framebuffer.info.width / 3U
                                       ? green
                                       : blue);

                        if (x >= 480 && x < 544 && y >= 352 && y < 416)
                                color = white;
                        row[x] = color;
                }
        }

        /* The permanent kernel alias owns this resource pin until shutdown. */
        frog_test_sync("framebuffer-ready");
        local_irq_disable();
        for (;;)
                __asm__ volatile("hlt");
}
#else
void framebuffer_smoke_run(void)
{
}
#endif
