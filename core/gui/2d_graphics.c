#include <sys/2d_graphics.h>

#include <const.h>
#include <debug.h>
#include <global.h>
#include <math.h>
#include <oslib.h>
#include <sys/memory.h>
// globla VBE mode structure
vbe_mode_info_t *g_gfx_mode;

BOOT_GFX_MODE_t boot_graphics_mode(void)
{
    g_gfx_mode = *((vbe_mode_info_t **) VBE_MODE_INFO_POINTER);
    vbe_info_t *vbe_info = *((struct vbe_info_structure **) VBE_INFO_POINTER);
    if (g_gfx_mode == 0 && vbe_info == 0) {
        return BOOT_VGA_MODE;
    } else if ((uint_32) g_gfx_mode == 1 && (uint_32) vbe_info == 1) {
        return BOOT_CGA_MODE;
    } else if ((g_gfx_mode && (uint_32) g_gfx_mode != 1) ||
               (vbe_info && (uint_32) vbe_info != 1)) {
        return BOOT_VBE_MODE;
    } else {
        return BOOT_UNKNOW;
    }
}

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

static void draw_2d_gfx_8bit_font(int font_size,
                                  int_16 x,
                                  int_16 y,
                                  uint_32 color,
                                  char *font)
{
    char font_part_8bit;  // each line font data
    for (int i = 0; i < 16; i++) {
        // go though all bits each line has 8 bits
        font_part_8bit = font[i];
        int_8 bit_idx = 0;
        while (bit_idx < 8) {
            if (font_part_8bit & 0x01) {
                draw_pixel(x + (7 - bit_idx), (y + i), color);
            }
            bit_idx++;
            font_part_8bit >>= 1;
        }
    }
    return;
}

void draw_2d_gfx_asc_char(int font_size, int x, int y, uint_32 color, char c)
{
    ASSERT(font_size == 8 || font_size == 16 || font_size == 32);
    char *font_8bits = (char *) FONT_HANKAKU;
    if (font_size == 8) {  // font_width
        draw_2d_gfx_8bit_font(font_size, x, y, color, font_8bits + (c * 16));
    } else if (font_size == 16) {
    } else if (font_size == 32) {
    } else {  // use default font size
    }
}

void draw_2d_gfx_string(int font_size,
                        int x,
                        int y,
                        uint_32 color,
                        char *str,
                        uint_32 str_len)
{
    for (int_32 char_idx = 0; char_idx < str_len; char_idx++) {
        draw_2d_gfx_asc_char(font_size, x + font_size * char_idx, y, color,
                             *(str + char_idx));
    }
}

/**
 * draw a hex number
 * return this number length
 *
 * @param param write here param Comments write here
 * @return length of this number
 *****************************************************************************/
uint_32 draw_2d_gfx_hex(int font_size, int x, int y, uint_32 color, int_32 num)
{
    char num_str[20] = {0};
    uint_32 len = itoa(num, num_str, 16);
    draw_2d_gfx_string(font_size, x, y, color, num_str, len);
    return len;
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
//
// Draw a line
//   Adapted from Wikipedia page on Bresenham's line algorithm
void draw_line(Point start, Point end, uint_32 color)
{
    int_16 deltaX =
        ABS((end.X -
             start.X));  // Delta X, change in X values, positive absolute value
    int_16 deltaY = -ABS(
        (end.Y -
         start.Y));  // Delta Y, change in Y values, negative absolute value
    int_16 signX =
        (start.X < end.X) ? 1 : -1;  // Sign of X direction, moving right
                                     // (positive) or left (negative)
    int_16 signY =
        (start.Y < end.Y) ? 1 : -1;  // Sign of Y direction, moving down
                                     // (positive) or up (negative)
    int_16 error = deltaX + deltaY;
    int_16 errorX2;

    while (1) {
        draw_pixel(start.X, start.Y, color);
        if (start.X == end.X && start.Y == end.Y)
            break;

        errorX2 = error * 2;
        if (errorX2 >= deltaY) {
            error += deltaY;
            start.X += signX;
        }
        if (errorX2 <= deltaX) {
            error += deltaX;
            start.Y += signY;
        }
    }
}

// Draw a triangle
void draw_triangle(Point vertex0, Point vertex1, Point vertex2, uint_32 color)
{
    // Draw lines between all 3 points
    draw_line(vertex0, vertex1, color);
    draw_line(vertex1, vertex2, color);
    draw_line(vertex2, vertex0, color);
}

// Draw a rectangle
void draw_rect(Point top_left, Point bottom_right, uint_32 color)
{
    Point temp = bottom_right;

    // Draw 4 lines, 2 horizontal parallel sides, 2 vertical parallel sides
    // Top of rectangle
    temp.Y = top_left.Y;
    draw_line(top_left, temp, color);

    // Right side
    draw_line(temp, bottom_right, color);

    // Bottom
    temp.X = top_left.X;
    temp.Y = bottom_right.Y;
    draw_line(bottom_right, temp, color);

    // Left side
    draw_line(temp, top_left, color);
}

// Draw a polygon
void draw_polygon(Point vertex_array[], uint_8 num_vertices, uint_32 color)
{
    // Draw lines up to last line
    for (uint_8 i = 0; i < num_vertices - 1; i++)
        draw_line(vertex_array[i], vertex_array[i + 1], color);

    // Draw last line
    draw_line(vertex_array[num_vertices - 1], vertex_array[0], color);
}

// Draw a circle
//  Adapted from "Computer Graphics Principles and Practice in C 2nd Edition"
void draw_circle(Point center, uint_16 radius, uint_32 color)
{
    int_16 x = 0;
    int_16 y = radius;
    int_16 p = 1 - radius;

    // Draw initial 8 octant points
    draw_pixel(center.X + x, center.Y + y, color);
    draw_pixel(center.X - x, center.Y + y, color);
    draw_pixel(center.X + x, center.Y - y, color);
    draw_pixel(center.X - x, center.Y - y, color);
    draw_pixel(center.X + y, center.Y + x, color);
    draw_pixel(center.X - y, center.Y + x, color);
    draw_pixel(center.X + y, center.Y - x, color);
    draw_pixel(center.X - y, center.Y - x, color);

    while (x < y) {
        x++;
        if (p < 0)
            p += 2 * x + 1;
        else {
            y--;
            p += 2 * (x - y) + 1;
        }

        // Draw next set of 8 octant points
        draw_pixel(center.X + x, center.Y + y, color);
        draw_pixel(center.X - x, center.Y + y, color);
        draw_pixel(center.X + x, center.Y - y, color);
        draw_pixel(center.X - x, center.Y - y, color);
        draw_pixel(center.X + y, center.Y + x, color);
        draw_pixel(center.X - y, center.Y + x, color);
        draw_pixel(center.X + y, center.Y - x, color);
        draw_pixel(center.X - y, center.Y - x, color);
    }
}

// Draw an ellipse
// TODO: Put this back in later, using non-floating point values/ROUND if
// possible
//  Adapted from "Computer Graphics Principles and Practice in C 2nd Edition"
// void draw_ellipse(Point center, uint_16 radiusX, uint_16 radiusY, uint_32
// color)
//{
//    int32_t rx2 = radiusX * radiusX;
//    int32_t ry2 = radiusY * radiusY;
//    int32_t twoRx2 = 2*rx2;
//    int32_t twoRy2 = 2*ry2;
//    int32_t p;
//    int32_t x = 0;
//    int32_t y = radiusY;
//    int32_t px = 0;
//    int32_t py = twoRx2 * y;
//
//    // Draw initial 4 quadrant points
//    draw_pixel(center.X + x, center.Y + y, color);
//    draw_pixel(center.X - x, center.Y + y, color);
//    draw_pixel(center.X + x, center.Y - y, color);
//    draw_pixel(center.X - x, center.Y - y, color);
//
//    // Region 1
//    p = ROUND(ry2 - (rx2 * radiusY) + (0.25 * rx2));
//    while (px < py) {
//        x++;
//        px += twoRy2;
//        if (p < 0) p += ry2 + px;
//        else {
//            y--;
//            py -= twoRx2;
//            p += ry2 + px - py;
//        }
//
//        // Draw 4 quadrant points
//        draw_pixel(center.X + x, center.Y + y, color);
//        draw_pixel(center.X - x, center.Y + y, color);
//        draw_pixel(center.X + x, center.Y - y, color);
//        draw_pixel(center.X - x, center.Y - y, color);
//    }
//
//    // Region 2
//    p = ROUND(ry2 * (x + 0.5)*(x + 0.5) + rx2 * (y - 1) * (y - 1) - rx2*ry2);
//    while (y > 0) {
//        y--;
//        py -= twoRx2; if (p > 0) p += rx2 - py;
//        else {
//            x++;
//            px += twoRy2;
//            p += rx2 - py + px;
//        }
//
//        // Draw 4 quadrant points
//        draw_pixel(center.X + x, center.Y + y, color);
//        draw_pixel(center.X - x, center.Y + y, color);
//        draw_pixel(center.X + x, center.Y - y, color);
//        draw_pixel(center.X - x, center.Y - y, color);
//    }
//}

// Fill an area with a solid color
void boundary_fill(uint_16 X,
                   uint_16 Y,
                   uint_32 fill_color,
                   uint_32 boundary_color)
{
    // Recursive - may use a lot of stack space
    uint_8 *framebuffer = (uint_8 *) g_gfx_mode->physical_base_pointer;
    uint_8 bytes_per_pixel =
        (g_gfx_mode->bits_per_pixel + 1) /
        8;  // Get # of bytes per pixel, add 1 to fix 15bpp modes
    uint_8 draw = 0;

    framebuffer += (Y * g_gfx_mode->x_resolution + X) * bytes_per_pixel;

    for (uint_8 temp = 0; temp < bytes_per_pixel; temp++) {
        if ((framebuffer[temp] != (uint_8) (fill_color >> (temp * 8))) &&
            (framebuffer[temp] != (uint_8) (boundary_color >> (temp * 8)))) {
            draw = 1;
            break;
        }
    }

    if (draw) {
        for (uint_8 temp = 0; temp < bytes_per_pixel; temp++)
            framebuffer[temp] = (uint_8) (fill_color >> temp * 8);

        // Check 4 pixels around current pixel
        boundary_fill(X + 1, Y, fill_color, boundary_color);
        boundary_fill(X - 1, Y, fill_color, boundary_color);
        boundary_fill(X, Y + 1, fill_color, boundary_color);
        boundary_fill(X, Y - 1, fill_color, boundary_color);
    }
}


// Fill triangle with a solid color
void fill_triangle_solid(Point p0, Point p1, Point p2, uint_32 color)
{
    Point temp;

    // Get center (Centroid) of a triangle
    temp.X = (p0.X + p1.X + p2.X) / 3;
    temp.Y = (p0.Y + p1.Y + p2.Y) / 3;

    // First draw triangles sides (boundaries)
    draw_triangle(p0, p1, p2, color - 1);

    // Then fill in boundaries
    boundary_fill(temp.X, temp.Y, color, color - 1);

    // Then redraw boundaries to correct color
    draw_triangle(p0, p1, p2, color);
}

// Fill rectangle with a solid color
void fill_rect_solid(Point top_left, Point bottom_right, uint_32 color)
{
    // Brute force method
    for (uint_16 y = top_left.Y; y < bottom_right.Y; y++)
        for (uint_16 x = top_left.X; x < bottom_right.X; x++)
            draw_pixel(x, y, color);
}

// Fill a polygon with a solid color
void fill_polygon_solid(Point vertex_array[],
                        uint_8 num_vertices,
                        uint_32 color)
{
    Point temp;

    // Assuming this works in general, get center (Centroid) of a triangle in
    // the polygon
    temp.X = (vertex_array[0].X + vertex_array[1].X + vertex_array[2].X) / 3;
    temp.Y = (vertex_array[0].Y + vertex_array[1].Y + vertex_array[2].Y) / 3;

    // First draw polygon sides (boundaries)
    draw_polygon(vertex_array, num_vertices, color - 1);

    // Then fill in boundaries
    boundary_fill(temp.X, temp.Y, color, color - 1);

    // Then redraw boundaries to correct color
    draw_polygon(vertex_array, num_vertices, color);
}

// Fill a circle with a solid color
void fill_circle_solid(Point center, uint_16 radius, uint_32 color)
{
    // Brute force method
    for (int_16 y = -radius; y <= radius; y++)
        for (int_16 x = -radius; x <= radius; x++)
            if (x * x + y * y < radius * radius - radius)
                draw_pixel(center.X + x, center.Y + y, color);
}

// Fill an ellipse with a solid color
// TODO: Put this back in later, using non-floating point values/ROUND if
// possible
// void fill_ellipse_solid(Point center, uint_16 radiusX, uint_16 radiusY,
// uint_32 color)
//{
//    // First draw boundaries a different color
//    draw_ellipse(center, radiusX, radiusY, color - 1);
//
//    // Then fill in the boundaries
//    boundary_fill(center.X, center.Y, color, color - 1);
//
//    // Then redraw boundaries as the correct color
//    draw_ellipse(center, radiusX, radiusY, color);
//}
uint_32 fetch_color(uint_32 X, uint_32 Y)
{
    uint_8 *framebuffer = (uint_8 *) g_gfx_mode->physical_base_pointer;
    uint_8 bytes_per_pixel =
        (g_gfx_mode->bits_per_pixel + 1) /
        8;  // Get # of bytes per pixel, add 1 to fix 15bpp modes

    framebuffer += (Y * g_gfx_mode->x_resolution + X) * bytes_per_pixel;

    return *(uint_32 *)framebuffer;
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

void draw_2d_gfx_cursor(uint_32 pos_x, uint_32 pos_y, uint_32 *color)
{
    Point size = {.X = 4, .Y = 4};
    Point topleft = {.X = pos_x, .Y = pos_y};
    Point downright = {.X = pos_x + size.X, .Y = pos_y + size.Y};
    uint_32 defalut_cursor_color = convert_color(FSK_DEEP_PINK);
    if (color) {
        fill_rect_solid(topleft, downright, *color);
    } else {
        fill_rect_solid(topleft, downright, defalut_cursor_color);
    }
}
