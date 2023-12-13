#include <sys/2d_graphics.h>

#include <const.h>
#include <global.h>
#include <math.h>
#include <sys/memory.h>
// globla VBE mode structure
vbe_mode_info_t *g_gfx_mode;

void twoD_graphics_init(void)
{
    // alloc 2d graphics memory
    g_gfx_mode = *((vbe_mode_info_t **) VBE_MODE_INFO_POINTER);
    vbe_info_t *vbe_info = *((struct vbe_info_structure **) VBE_INFO_POINTER);
    const uint_32 fbsize_in_bytes =
        g_gfx_mode->y_resolution * g_gfx_mode->linear_bytes_per_scanline;
    uint_32 fb_page_count = DIV_ROUND_UP(fbsize_in_bytes, PG_SIZE);
    // For hardware, double size of framebuffer pages just in case
    fb_page_count *= 2;
    for (uint_32 i = 0, fb_start = g_gfx_mode->physical_base_pointer;
         i < fb_page_count; i++, fb_start += PG_SIZE)
        put_page((void *) fb_start, (void *) fb_start);

    // test VBE info
    if (!strcmp("VESA", vbe_info->signature) && vbe_info != NULL) {
        // has vbe_info and right signature
        uint_32 mode = vbe_info->video_modes_pointer;
        uint_32 vers = vbe_info->version;
        switch (vbe_info->version) {
        case 0x100:
            // VBE 1.0
            break;
        case 0x200:
            // VBE 2.0
            break;
        case 0x300:
            // VBE 3.0
            break;
        default:
            // VBE unknow
            break;
        }
    }
}

// Convert given 32bit 888ARGB color to set bpp value
// 0x00RRGGBB
uint_32 convert_color(const uint_32 color)
{
    uint_8 convert_r, convert_g, convert_b;
    uint_32 converted_color = 0;

    // Get original color portions
    const uint_8 orig_r = (color >> 16) & 0xFF;
    const uint_8 orig_g = (color >> 8) & 0xFF;
    const uint_8 orig_b = color & 0xFF;

    if (g_gfx_mode->bits_per_pixel == 8) {
        // 8bpp uses standard VGA 256 color pallette
        // User can enter any 8bit value for color 0x00-0xFF
        convert_r = 0;
        convert_g = 0;
        convert_b = orig_b;
    } else {
        // Assuming bpp is > 8 and <= 32
        const uint_8 r_bits_to_shift = 8 - g_gfx_mode->linear_red_mask_size;
        const uint_8 g_bits_to_shift = 8 - g_gfx_mode->linear_green_mask_size;
        const uint_8 b_bits_to_shift = 8 - g_gfx_mode->linear_blue_mask_size;

        // Convert to new color portions by getting ratio of bit sizes of color
        // compared to "full" 8 bit colors
        convert_r = (orig_r >> r_bits_to_shift) &
                    ((1 << g_gfx_mode->linear_red_mask_size) - 1);
        convert_g = (orig_g >> g_bits_to_shift) &
                    ((1 << g_gfx_mode->linear_green_mask_size) - 1);
        convert_b = (orig_b >> b_bits_to_shift) &
                    ((1 << g_gfx_mode->linear_blue_mask_size) - 1);
    }

    // Put new color portions into new color
    converted_color = (convert_r << g_gfx_mode->linear_red_field_position) |
                      (convert_g << g_gfx_mode->linear_green_field_position) |
                      (convert_b << g_gfx_mode->linear_blue_field_position);

    return converted_color;
}
// Draw a single pixel
void draw_pixel(uint_16 X, uint_16 Y, uint_32 color)
{
    uint_8 *framebuffer = (uint_8 *) g_gfx_mode->physical_base_pointer;
    uint_8 bytes_per_pixel =
        (g_gfx_mode->bits_per_pixel + 1) /
        8;  // Get # of bytes per pixel, add 1 to fix 15bpp modes

    framebuffer += (Y * g_gfx_mode->x_resolution + X) * bytes_per_pixel;

    for (uint_8 temp = 0; temp < bytes_per_pixel; temp++)
        framebuffer[temp] = (uint_8) (color >> temp * 8);
}

void clear_screen(uint_32 color)
{
    // Get 32bit pointer to framebuffer value in VBE mode info block,
    //   dereference to get the 32bit value,
    //   get 32bit pointer to that value - memory address of framebuffer
    uint_8 *framebuffer = (uint_8 *) g_gfx_mode->physical_base_pointer;
    uint_8 bytes_per_pixel = (g_gfx_mode->bits_per_pixel + 1) / 8;

    for (uint_32 i = 0; i < g_gfx_mode->x_resolution * g_gfx_mode->y_resolution;
         i++) {
        *((uint_32 *) framebuffer) = color;

        framebuffer += bytes_per_pixel;
    }
}
