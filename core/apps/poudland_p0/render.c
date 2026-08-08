#include "poudland_p0.h"

static uint_8 blend_channel(uint_8 foreground, uint_8 background,
                            uint_32 alpha)
{
        return (uint_8) (((uint_32) foreground * alpha +
                          (uint_32) background * (255U - alpha) + 127U) /
                         255U);
}

static uint_32 blend_pixel(uint_32 foreground, uint_32 background)
{
        uint_32 alpha = foreground >> 24;
        uint_8 red;
        uint_8 green;
        uint_8 blue;

        if (alpha == 0)
                return background;
        if (alpha == 255U)
                return foreground & 0x00ffffffU;
        red = blend_channel((foreground >> 16) & 0xffU,
                            (background >> 16) & 0xffU, alpha);
        green = blend_channel((foreground >> 8) & 0xffU,
                              (background >> 8) & 0xffU, alpha);
        blue = blend_channel(foreground & 0xffU, background & 0xffU, alpha);
        return ((uint_32) red << 16) | ((uint_32) green << 8) | blue;
}

bool poudland_p0_rect_clip(struct poudland_p0_rect *rect,
                           uint_32 width, uint_32 height)
{
        long long left;
        long long top;
        long long right;
        long long bottom;

        if (rect == NULL || rect->width <= 0 || rect->height <= 0)
                return false;
        left = rect->x;
        top = rect->y;
        right = left + rect->width;
        bottom = top + rect->height;
        if (right <= 0 || bottom <= 0 || left >= width || top >= height)
                return false;
        if (left < 0)
                left = 0;
        if (top < 0)
                top = 0;
        if (right > width)
                right = width;
        if (bottom > height)
                bottom = height;
        rect->x = (int_32) left;
        rect->y = (int_32) top;
        rect->width = (int_32) (right - left);
        rect->height = (int_32) (bottom - top);
        return rect->width > 0 && rect->height > 0;
}

void poudland_p0_damage(struct poudland_p0_display *display,
                        struct poudland_p0_rect rect)
{
        int_32 left;
        int_32 top;
        int_32 right;
        int_32 bottom;

        if (display == NULL ||
            !poudland_p0_rect_clip(&rect, display->info.width,
                                   display->info.height))
                return;
        display->damage_requests++;
        if (!display->damaged) {
                display->damage = rect;
                display->damaged = true;
                return;
        }
        left = rect.x < display->damage.x ? rect.x : display->damage.x;
        top = rect.y < display->damage.y ? rect.y : display->damage.y;
        right = rect.x + rect.width;
        if (display->damage.x + display->damage.width > right)
                right = display->damage.x + display->damage.width;
        bottom = rect.y + rect.height;
        if (display->damage.y + display->damage.height > bottom)
                bottom = display->damage.y + display->damage.height;
        display->damage.x = left;
        display->damage.y = top;
        display->damage.width = right - left;
        display->damage.height = bottom - top;
}

static bool pixel_in_rect(int_32 x, int_32 y,
                          const struct poudland_p0_rect *rect)
{
        return x >= rect->x && y >= rect->y &&
               x < rect->x + rect->width &&
               y < rect->y + rect->height;
}

static bool pixel_on_border(int_32 x, int_32 y,
                            const struct poudland_p0_rect *rect)
{
        const int_32 border = 3;

        return pixel_in_rect(x, y, rect) &&
               (x < rect->x + border || y < rect->y + border ||
                x >= rect->x + rect->width - border ||
                y >= rect->y + rect->height - border);
}

static uint_32 scene_pixel(const struct poudland_p0_scene *scene,
                           int_32 x, int_32 y)
{
        uint_32 pixel = 0x00204060U;
        uint_32 i;
        int_32 cursor_x;
        int_32 cursor_y;

        for (i = 0; i < POUDLAND_P0_WINDOW_COUNT; ++i) {
                if (!pixel_in_rect(x, y, &scene->windows[i].bounds))
                        continue;
                pixel = scene->windows[i].color;
                if ((int_32) i == scene->focused_window &&
                    pixel_on_border(x, y, &scene->windows[i].bounds))
                        pixel = 0x00ffffffU;
        }
        cursor_x = x - scene->cursor_x;
        cursor_y = y - scene->cursor_y;
        if (cursor_x >= 0 && cursor_y >= 0 &&
            (uint_32) cursor_x < scene->cursor.width &&
            (uint_32) cursor_y < scene->cursor.height)
                pixel = blend_pixel(scene->cursor.pixels[
                    (uint_32) cursor_y * scene->cursor.width + cursor_x],
                    pixel);
        return pixel;
}

static void write_pixel(volatile uint_8 *destination, uint_32 value,
                        uint_32 bytes_per_pixel)
{
        uint_32 byte;

        for (byte = 0; byte < bytes_per_pixel; ++byte)
                destination[byte] = (value >> (byte * 8U)) & 0xffU;
}

bool poudland_p0_render(struct poudland_p0_scene *scene)
{
        struct poudland_p0_display *display;
        struct poudland_p0_rect damage;
        uint_32 bytes_per_pixel;
        int_32 x;
        int_32 y;

        if (scene == NULL || !scene->display.damaged)
                return false;
        display = &scene->display;
        damage = display->damage;
        if (!poudland_p0_rect_clip(&damage, display->info.width,
                                   display->info.height)) {
                display->damaged = false;
                return false;
        }
        bytes_per_pixel = display->info.bits_per_pixel / 8U;
        for (y = damage.y; y < damage.y + damage.height; ++y) {
                for (x = damage.x; x < damage.x + damage.width; ++x) {
                        uint_32 xrgb = scene_pixel(scene, x, y);
                        uint_32 offset = (uint_32) y * display->info.width +
                                         (uint_32) x;
                        uint_32 packed = poudland_p0_pack_pixel(
                            &display->info, xrgb);
                        volatile uint_8 *destination = display->framebuffer +
                            (uint_32) y * display->info.pitch +
                            (uint_32) x * bytes_per_pixel;

                        display->backbuffer[offset] = xrgb;
                        write_pixel(destination, packed, bytes_per_pixel);
                }
        }
        display->frame_count++;
        display->presented_pixels +=
            (uint_32) damage.width * (uint_32) damage.height;
        display->damaged = false;
        return true;
}
