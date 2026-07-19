#include <frog/irqflags.h>
#include <frog/math.h>
#include <frog/memory.h>
#include <frog/types.h>
#include <kernel/framebuffer_smoke.h>
#include <kernel/qemu_test.h>
#include <video/video.h>

#define VBE_MODE_INFO_POINTER 0x0ffcUL
#define BOOT_HANDOFF_LOW_START 0x0500UL
#define BOOT_HANDOFF_LOW_END 0x100000UL
#define FRAMEBUFFER_TEST_WIDTH 1024
#define FRAMEBUFFER_TEST_HEIGHT 768

#ifdef CONFIG_FROG_TEST_FRAMEBUFFER
static uint_32 channel_mask(uint_8 width)
{
        if (width == 0 || width > 8)
                return 0;
        return (1UL << width) - 1;
}

static int valid_channel(uint_8 width, uint_8 position)
{
        return width > 0 && width <= 8 && position < 32 &&
               width <= 32 - position;
}

static int channels_overlap(uint_8 first_width, uint_8 first_position,
                            uint_8 second_width, uint_8 second_position)
{
        uint_32 first = channel_mask(first_width) << first_position;
        uint_32 second = channel_mask(second_width) << second_position;
        return first & second;
}

static uint_16 framebuffer_stride(const vbe_mode_info_t *mode)
{
        return mode->linear_bytes_per_scanline
                   ? mode->linear_bytes_per_scanline
                   : mode->bytes_per_scanline;
}

static uint_8 red_width(const vbe_mode_info_t *mode)
{
        return mode->linear_red_mask_size ? mode->linear_red_mask_size
                                          : mode->red_mask_size;
}

static uint_8 red_position(const vbe_mode_info_t *mode)
{
        return mode->linear_red_mask_size ? mode->linear_red_field_position
                                          : mode->red_field_position;
}

static uint_8 green_width(const vbe_mode_info_t *mode)
{
        return mode->linear_green_mask_size ? mode->linear_green_mask_size
                                            : mode->green_mask_size;
}

static uint_8 green_position(const vbe_mode_info_t *mode)
{
        return mode->linear_green_mask_size ? mode->linear_green_field_position
                                            : mode->green_field_position;
}

static uint_8 blue_width(const vbe_mode_info_t *mode)
{
        return mode->linear_blue_mask_size ? mode->linear_blue_mask_size
                                           : mode->blue_mask_size;
}

static uint_8 blue_position(const vbe_mode_info_t *mode)
{
        return mode->linear_blue_mask_size ? mode->linear_blue_field_position
                                           : mode->blue_field_position;
}

static uint_32 encode_rgb(const vbe_mode_info_t *mode,
                          uint_8 red,
                          uint_8 green,
                          uint_8 blue)
{
        uint_32 r = (red >> (8 - red_width(mode))) &
                    channel_mask(red_width(mode));
        uint_32 g = (green >> (8 - green_width(mode))) &
                    channel_mask(green_width(mode));
        uint_32 b = (blue >> (8 - blue_width(mode))) &
                    channel_mask(blue_width(mode));
        return (r << red_position(mode)) | (g << green_position(mode)) |
               (b << blue_position(mode));
}

static int valid_mode(const vbe_mode_info_t *mode)
{
        if (mode == NULL || (uintptr_t) mode <= 1)
                return 0;
        if ((mode->mode_attributes & 0x0099) != 0x0099 ||
            mode->x_resolution != FRAMEBUFFER_TEST_WIDTH ||
            mode->y_resolution != FRAMEBUFFER_TEST_HEIGHT ||
            mode->number_of_planes != 1 || mode->memory_model != 6 ||
            mode->bits_per_pixel != 32 ||
            mode->physical_base_pointer == 0)
                return 0;

        uint_16 stride = framebuffer_stride(mode);
        uint_8 rw = red_width(mode);
        uint_8 rp = red_position(mode);
        uint_8 gw = green_width(mode);
        uint_8 gp = green_position(mode);
        uint_8 bw = blue_width(mode);
        uint_8 bp = blue_position(mode);

        if (stride < mode->x_resolution * 4 ||
            !valid_channel(rw, rp) || !valid_channel(gw, gp) ||
            !valid_channel(bw, bp) || channels_overlap(rw, rp, gw, gp) ||
            channels_overlap(rw, rp, bw, bp) ||
            channels_overlap(gw, gp, bw, bp))
                return 0;

        uint_32 framebuffer_size = stride * mode->y_resolution;
        return framebuffer_size / mode->y_resolution ==
               stride;
}

void framebuffer_smoke_run(void)
{
        uintptr_t mode_address = *(uintptr_t *) VBE_MODE_INFO_POINTER;
        if (mode_address < BOOT_HANDOFF_LOW_START ||
            mode_address > BOOT_HANDOFF_LOW_END - sizeof(vbe_mode_info_t))
                frog_test_abort("framebuffer-mode-pointer-invalid");

        vbe_mode_info_t *mode = (vbe_mode_info_t *) mode_address;
        if (!valid_mode(mode))
                frog_test_abort("framebuffer-mode-invalid");

        uint_32 page_offset = mode->physical_base_pointer & (PAGE_SIZE - 1);
        uint_32 physical_base = mode->physical_base_pointer & ~(PAGE_SIZE - 1);
        uint_32 framebuffer_size =
            framebuffer_stride(mode) * mode->y_resolution;
        if (framebuffer_size > 16 * 1024 * 1024UL - page_offset ||
            map_kernel_framebuffer(physical_base,
                                   framebuffer_size + page_offset) < 0)
                frog_test_abort("framebuffer-map-failed");

        uint_8 *framebuffer =
            (uint_8 *) (KERNEL_FRAMEBUFFER_VADDR + page_offset);
        uint_32 red = encode_rgb(mode, 255, 0, 0);
        uint_32 green = encode_rgb(mode, 0, 255, 0);
        uint_32 blue = encode_rgb(mode, 0, 0, 255);
        uint_32 white = encode_rgb(mode, 255, 255, 255);

        for (uint_32 y = 0; y < mode->y_resolution; y++) {
                uint_32 *row =
                    (uint_32 *) (framebuffer + y * framebuffer_stride(mode));
                for (uint_32 x = 0; x < mode->x_resolution; x++) {
                        uint_32 color = x < mode->x_resolution / 3
                                            ? red
                                            : (x < 2 * mode->x_resolution / 3
                                                   ? green
                                                   : blue);
                        if (x >= 480 && x < 544 && y >= 352 && y < 416)
                                color = white;
                        row[x] = color;
                }
        }

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
