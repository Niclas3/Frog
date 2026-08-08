#include "poudland_p0.h"

#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/mman.h>
#include <frog/syscall.h>

#define POUDLAND_P0_PAGE_SIZE 4096U
#define POUDLAND_P0_MAP_LIMIT (16U * 1024U * 1024U)

static bool channel_valid(uint_32 size, uint_32 position,
                          uint_32 bits_per_pixel)
{
        return size > 0 && size <= 8 && position < bits_per_pixel &&
               size <= bits_per_pixel - position;
}

static uint_32 channel_mask(uint_32 size, uint_32 position)
{
        return ((1U << size) - 1U) << position;
}

bool poudland_p0_display_info_valid(const struct frog_fb_info *info)
{
        uint_32 bytes_per_pixel;
        unsigned long long visible;
        uint_32 red_mask;
        uint_32 green_mask;
        uint_32 blue_mask;

        if (info == NULL || info->width == 0 || info->height == 0 ||
            (info->bits_per_pixel != 8U &&
             info->bits_per_pixel != 16U &&
             info->bits_per_pixel != 32U))
                return false;
        bytes_per_pixel = info->bits_per_pixel / 8U;
        visible = (unsigned long long) info->pitch * info->height;
        if (info->width > 0xffffffffU / bytes_per_pixel ||
            info->pitch < info->width * bytes_per_pixel ||
            visible != info->visible_length ||
            info->map_length < info->visible_length ||
            info->map_length > POUDLAND_P0_MAP_LIMIT ||
            (info->map_length & (POUDLAND_P0_PAGE_SIZE - 1U)) != 0)
                return false;
        if (!channel_valid(info->red_size, info->red_position,
                           info->bits_per_pixel) ||
            !channel_valid(info->green_size, info->green_position,
                           info->bits_per_pixel) ||
            !channel_valid(info->blue_size, info->blue_position,
                           info->bits_per_pixel))
                return false;
        red_mask = channel_mask(info->red_size, info->red_position);
        green_mask = channel_mask(info->green_size, info->green_position);
        blue_mask = channel_mask(info->blue_size, info->blue_position);
        return (red_mask & green_mask) == 0 &&
               (red_mask & blue_mask) == 0 &&
               (green_mask & blue_mask) == 0;
}

static uint_32 scale_channel(uint_32 value, uint_32 size)
{
        uint_32 maximum = (1U << size) - 1U;

        return (value * maximum + 127U) / 255U;
}

uint_32 poudland_p0_pack_pixel(const struct frog_fb_info *info,
                              uint_32 xrgb)
{
        uint_32 red = (xrgb >> 16) & 0xffU;
        uint_32 green = (xrgb >> 8) & 0xffU;
        uint_32 blue = xrgb & 0xffU;

        return (scale_channel(red, info->red_size) << info->red_position) |
               (scale_channel(green, info->green_size) <<
                info->green_position) |
               (scale_channel(blue, info->blue_size) <<
                info->blue_position);
}

bool poudland_p0_format_self_test(void)
{
        struct frog_fb_info rgb332 = {
            .bits_per_pixel = 8,
            .red_position = 5, .red_size = 3,
            .green_position = 2, .green_size = 3,
            .blue_position = 0, .blue_size = 2,
        };
        struct frog_fb_info rgb565 = {
            .bits_per_pixel = 16,
            .red_position = 11, .red_size = 5,
            .green_position = 5, .green_size = 6,
            .blue_position = 0, .blue_size = 5,
        };
        struct frog_fb_info xrgb8888 = {
            .bits_per_pixel = 32,
            .red_position = 16, .red_size = 8,
            .green_position = 8, .green_size = 8,
            .blue_position = 0, .blue_size = 8,
        };

        return poudland_p0_pack_pixel(&rgb332, 0x00ff0000U) == 0xe0U &&
               poudland_p0_pack_pixel(&rgb332, 0x0000ff00U) == 0x1cU &&
               poudland_p0_pack_pixel(&rgb332, 0x000000ffU) == 0x03U &&
               poudland_p0_pack_pixel(&rgb565, 0x00ff0000U) == 0xf800U &&
               poudland_p0_pack_pixel(&rgb565, 0x0000ff00U) == 0x07e0U &&
               poudland_p0_pack_pixel(&rgb565, 0x000000ffU) == 0x001fU &&
               poudland_p0_pack_pixel(&xrgb8888, 0x00123456U) ==
                   0x00123456U;
}

int_32 poudland_p0_display_open(struct poudland_p0_display *display)
{
        unsigned long long backbuffer_bytes;
        uint_32 rounded_length;

        if (display == NULL)
                return -EINVAL;
        display->fd = -1;
        display->framebuffer = MAP_FAILED;
        display->backbuffer = MAP_FAILED;
        display->fd = open("/dev/fb0", O_RDWR);
        if (display->fd < 0)
                return display->fd;
        int_32 result = ioctl(display->fd, FROG_FB_IOCTL_GET_INFO,
                              &display->info);

        if (result != 0 || !poudland_p0_display_info_valid(&display->info)) {
                poudland_p0_display_close(display);
                return result != 0 ? result : -EINVAL;
        }
        display->framebuffer = mmap(
            NULL, display->info.map_length, PROT_READ | PROT_WRITE,
            MAP_SHARED, display->fd, 0);
        if (display->framebuffer == MAP_FAILED) {
                poudland_p0_display_close(display);
                return -ENOMEM;
        }
        backbuffer_bytes = (unsigned long long) display->info.width *
                           display->info.height * sizeof(uint_32);
        if (backbuffer_bytes == 0 ||
            backbuffer_bytes > POUDLAND_P0_MAP_LIMIT ||
            backbuffer_bytes > 0xffffffffU - (POUDLAND_P0_PAGE_SIZE - 1U)) {
                poudland_p0_display_close(display);
                return -EOVERFLOW;
        }
        rounded_length = ((uint_32) backbuffer_bytes +
                          POUDLAND_P0_PAGE_SIZE - 1U) &
                         ~(POUDLAND_P0_PAGE_SIZE - 1U);
        display->backbuffer = mmap(
            NULL, rounded_length, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (display->backbuffer == MAP_FAILED) {
                poudland_p0_display_close(display);
                return -ENOMEM;
        }
        display->backbuffer_length = rounded_length;
        return 0;
}

void poudland_p0_display_close(struct poudland_p0_display *display)
{
        if (display == NULL)
                return;
        if (display->backbuffer != MAP_FAILED &&
            display->backbuffer != NULL && display->backbuffer_length != 0)
                (void) munmap(display->backbuffer,
                             display->backbuffer_length);
        if (display->framebuffer != MAP_FAILED &&
            display->framebuffer != NULL && display->info.map_length != 0)
                (void) munmap((void *) display->framebuffer,
                             display->info.map_length);
        if (display->fd >= 0)
                (void) close(display->fd);
        display->fd = -1;
        display->framebuffer = MAP_FAILED;
        display->backbuffer = MAP_FAILED;
        display->backbuffer_length = 0;
}
