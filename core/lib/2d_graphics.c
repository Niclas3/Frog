#include <gua/2d_graphics.h>

#include <const.h>
#include <debug.h>
#include <global.h>
#include <kernel/video.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/memory.h>
#include <sys/syscall.h>

// globla VBE mode structure
static vbe_mode_info_t gfx_mode;

uint_32 rgb(uint_8 r, uint_8 g, uint_8 b)
{
    return 0xFF000000 | (r << 16) | (g << 8) | (b);
}

uint_32 rgba(uint_8 r, uint_8 g, uint_8 b, uint_8 a)
{
    return (a << 24U) | (r << 16) | (g << 8) | (b);
}

inline uint_32 alpha_blend_rgba(uint_32 bottom, uint_32 top)
{
    if (_A(bottom) == 0)
        return top;
    if (_A(top) == 255)
        return top;
    if (_A(top) == 0)
        return bottom;
    uint_8 a = _A(top);
    uint_16 t = 0xFF ^ a;
    uint_8 d_r =
        _R(top) + (((uint_32) (_R(bottom) * t + 0x80) * 0x101) >> 16UL);
    uint_8 d_g =
        _G(top) + (((uint_32) (_G(bottom) * t + 0x80) * 0x101) >> 16UL);
    uint_8 d_b =
        _B(top) + (((uint_32) (_B(bottom) * t + 0x80) * 0x101) >> 16UL);
    uint_8 d_a =
        _A(top) + (((uint_32) (_A(bottom) * t + 0x80) * 0x101) >> 16UL);
    return rgba(d_r, d_g, d_b, d_a);
}

// Convert given 32bit 888ARGB color to set bpp value
// 0x00RRGGBB
bbp_t convert_argb(const argb_t color)
{
    uint_8 convert_r, convert_g, convert_b;
    uint_8 convert_a;
    uint_32 converted_color = 0;
    uint_32 alpha_field_position = 24;
    // Get original color portions
    const uint_8 orig_a = (color >> 24U) & 0xFF;
    const uint_8 orig_r = (color >> 16) & 0xFF;
    const uint_8 orig_g = (color >> 8) & 0xFF;
    const uint_8 orig_b = color & 0xFF;

    if (gfx_mode.bits_per_pixel == 8) {
        // 8bpp uses standard VGA 256 color pallette
        // User can enter any 8bit value for color 0x00-0xFF
        convert_r = 0;
        convert_g = 0;
        convert_b = orig_b;
    } else {
        // Assuming bpp is > 8 and <= 32
        const uint_8 r_bits_to_shift = 8 - gfx_mode.linear_red_mask_size;
        const uint_8 g_bits_to_shift = 8 - gfx_mode.linear_green_mask_size;
        const uint_8 b_bits_to_shift = 8 - gfx_mode.linear_blue_mask_size;

        // Convert to new color portions by getting ratio of bit sizes of color
        // compared to "full" 8 bit colors
        convert_r = (orig_r >> r_bits_to_shift) &
                    ((1 << gfx_mode.linear_red_mask_size) - 1);
        convert_g = (orig_g >> g_bits_to_shift) &
                    ((1 << gfx_mode.linear_green_mask_size) - 1);
        convert_b = (orig_b >> b_bits_to_shift) &
                    ((1 << gfx_mode.linear_blue_mask_size) - 1);
    }

    // Put new color portions into new color
    converted_color = (convert_a << alpha_field_position) |
                      (convert_r << gfx_mode.linear_red_field_position) |
                      (convert_g << gfx_mode.linear_green_field_position) |
                      (convert_b << gfx_mode.linear_blue_field_position);

    return converted_color;
}

// Convert given 32bit 888ARGB color to set bpp value
// 0x00RRGGBB
argb_t convert_bbp(const bbp_t bbpcolor)
{
    uint_8 convert_r, convert_g, convert_b;
    uint_32 converted_color = 0;

    // Get original color portions
    const uint_8 orig_r = (bbpcolor >> gfx_mode.red_field_position) & 0xFF;
    const uint_8 orig_g = (bbpcolor >> gfx_mode.green_field_position) & 0xFF;
    const uint_8 orig_b = (bbpcolor >> gfx_mode.blue_field_position) & 0xFF;

    if (gfx_mode.bits_per_pixel == 8) {
        // 8bpp uses standard VGA 256 color pallette
        // User can enter any 8bit value for color 0x00-0xFF
        convert_r = 0;
        convert_g = 0;
        convert_b = orig_b;
    } else {
        // Assuming bpp is > 8 and <= 32
        const uint_8 r_bits_to_shift = 8 - gfx_mode.linear_red_mask_size;
        const uint_8 g_bits_to_shift = 8 - gfx_mode.linear_green_mask_size;
        const uint_8 b_bits_to_shift = 8 - gfx_mode.linear_blue_mask_size;

        // Convert to new color portions by getting ratio of bit sizes of color
        // compared to "full" 8 bit colors
        convert_r = (orig_r << r_bits_to_shift) &
                    ((1 << gfx_mode.linear_red_mask_size) - 1);
        convert_g = (orig_g << g_bits_to_shift) &
                    ((1 << gfx_mode.linear_green_mask_size) - 1);
        convert_b = (orig_b << b_bits_to_shift) &
                    ((1 << gfx_mode.linear_blue_mask_size) - 1);
    }

    // Put new color portions into new color
    converted_color = rgb(convert_r, convert_g, convert_b);

    return converted_color;
}

gfx_context_t *init_gfx_fullscreen(void)
{
    gfx_context_t *ctx = malloc(sizeof(gfx_context_t));
    if (!ctx) {
        return NULL;
    }
    int_32 lfb_fd = open("/dev/fb0", O_RDWR);
    if (lfb_fd == -1) {
        return NULL;
    }

    ioctl(lfb_fd, IO_VID_WIDTH, &ctx->width);
    ioctl(lfb_fd, IO_VID_HEIGHT, &ctx->height);
    ioctl(lfb_fd, IO_VID_DEPTH, &ctx->depth);
    ioctl(lfb_fd, IO_VID_STRIDE, &ctx->stride);
    ioctl(lfb_fd, IO_VID_ADDR, &ctx->buffer);
    ioctl(lfb_fd, IO_VID_VBE_MODE, &gfx_mode);

    ctx->size = ctx->height * ctx->stride;
    /* ctx->size = gfx_mode.y_resolution * gfx_mode.linear_bytes_per_scanline;
     */

    /* ctx->width = gfx_mode.x_resolution; */
    /* ctx->height = gfx_mode.y_resolution; */
    /* ctx->buffer = (char *) gfx_mode.physical_base_pointer; */
    /* ctx->stride = gfx_mode.linear_bytes_per_scanline; */
    /* ctx->depth = gfx_mode.bits_per_pixel;  // Get # of bytes per pixel, add 1
     * to */
    /*                                        // fix 15bpp modes */
    ctx->backbuffer = ctx->buffer;
    ctx->clips = NULL;
    return ctx;
}

gfx_context_t *init_gfx_fullscreen_double_buffer(void)
{
    gfx_context_t *out = init_gfx_fullscreen();
    if (!out)
        return NULL;
    out->backbuffer = malloc(out->size);
    if (!out->backbuffer)
        return NULL;
    return out;
}

sprite_t *create_sprite(uint_32 width, uint_32 height, int_32 alpha)
{
    sprite_t *out = malloc(sizeof(sprite_t));

    /*
    uint16_t width;
    uint16_t height;
    uint32_t * bitmap;
    uint32_t * masks;
    uint32_t blank;
    uint8_t  alpha;
    */

    out->width = width;
    out->height = height;
    out->bitmap = malloc(sizeof(uint_32) * out->width * out->height);
    out->masks = NULL;
    out->blank = 0x00000000;
    out->alpha = alpha;

    return out;
}

void sprite_free(sprite_t *sprite)
{
    if (sprite->masks) {
        free(sprite->masks);
    }
    free(sprite->bitmap);
    free(sprite);
}

void clear_buffer(gfx_context_t *ctx)
{
    memset(ctx->backbuffer, 0, ctx->size);
}

static inline int_32 _is_in_clip(gfx_context_t *ctx, int_32 y)
{
    if (!ctx->clips)
        return 1;
    if (y < 0 || y >= ctx->clips_size)
        return 1;
    return ctx->clips[y];
}

void gfx_add_clip(gfx_context_t *ctx, int_32 x, int_32 y, int_32 w, int_32 h)
{
    (void) x;
    (void) w;
    if (!ctx->clips) {
        ctx->clips = malloc(ctx->height);
        memset(ctx->clips, 0, ctx->height);
        ctx->clips_size = ctx->height;
    }
    for (int_32 i = MAX(y, 0); i < MIN(y + h, ctx->clips_size); i++) {
        ctx->clips[i] = 1;
    }
}

void gfx_clear_clip(gfx_context_t *ctx)
{
    if (ctx->clips) {
        memset(ctx->clips, 0, ctx->clips_size);
    }
}

void gfx_free_clip(gfx_context_t *ctx)
{
    void *tmp = ctx->clips;
    if (!tmp)
        return;
    ctx->clips = NULL;
    free(tmp);
}

inline void flip(gfx_context_t *ctx)
{
    if (ctx->clips) {
        for (uint_32 i = 0; i < ctx->height; i++) {
            if (_is_in_clip(ctx, i)) {
                memcpy(&ctx->buffer[i * GFX_S(ctx)],
                       &ctx->backbuffer[i * GFX_S(ctx)],
                       4 * ctx->width);  // 4 is r g b a (?)
            }
        }
    } else {
        memcpy(ctx->buffer, ctx->backbuffer, ctx->size);
    }
}

static void draw_2d_gfx_8bit_font(gfx_context_t *ctx,
                                  int font_size,
                                  int_16 x,
                                  int_16 y,
                                  argb_t color,
                                  char *font)
{
    char font_part_8bit;  // each line font data
    for (int i = 0; i < 16; i++) {
        // go though all bits each line has 8 bits
        font_part_8bit = font[i];
        int_8 bit_idx = 0;
        while (bit_idx < 8) {
            if (font_part_8bit & 0x01) {
                draw_pixel(ctx, x + (7 - bit_idx), (y + i),
                           convert_argb(color));
            }
            bit_idx++;
            font_part_8bit >>= 1;
        }
    }
    return;
}

uint_32 draw_2d_gfx_asc_char(gfx_context_t *ctx,
                             int font_size,
                             int x,
                             int y,
                             argb_t color,
                             char c)
{
    ASSERT(font_size == 8 || font_size == 16 || font_size == 32);
    char *font_8bits = (char *) FONT_HANKAKU;
    if (font_size == 8) {  // font_width
        draw_2d_gfx_8bit_font(ctx, font_size, x, y, color,
                              font_8bits + (c * 16));
    } else if (font_size == 16) {
        PANIC("not support 16 font size yet.");
        return 0;
    } else if (font_size == 32) {
        PANIC("not support 32 font size yet.");
        return 0;
    } else {  // use default font size
        PANIC("not support ?? font size yet.");
        return 0;
    }
    return font_size;
}

uint_32 draw_2d_gfx_string(gfx_context_t *ctx,
                           int font_size,
                           int x,
                           int y,
                           argb_t color,
                           char *str,
                           uint_32 str_len)
{
    uint_32 pixel_length = 0;
    for (int_32 char_idx = 0; char_idx < str_len; char_idx++) {
        pixel_length +=
            draw_2d_gfx_asc_char(ctx, font_size, x + font_size * char_idx, y,
                                 color, *(str + char_idx));
    }
    return pixel_length;
}

/**
 * draw a hex number
 * return this number length
 *
 * @param param write here param Comments write here
 * @return length of this number
 *****************************************************************************/
uint_32 draw_2d_gfx_hex(gfx_context_t *ctx,
                        int font_size,
                        int x,
                        int y,
                        argb_t color,
                        int_32 num)
{
    char num_str[20] = {0};
    uint_32 len = sprintf(num_str, "%x", num);
    uint_32 pix_len = 0;
    pix_len = draw_2d_gfx_string(ctx, font_size, x, y, color, num_str, len);
    return pix_len;
}

/**
 * draw a dec number
 * return this number length
 *
 * @param param write here param Comments write here
 * @return length of this number
 *****************************************************************************/
uint_32 draw_2d_gfx_dec(gfx_context_t *ctx,
                        int font_size,
                        int x,
                        int y,
                        argb_t color,
                        int_32 num)
{
    char num_str[20] = {0};
    uint_32 len = sprintf(num_str, "%d", num);
    uint_32 pix_len = 0;
    pix_len = draw_2d_gfx_string(ctx, font_size, x, y, color, num_str, len);
    return pix_len;
}

// Draw a single pixel
inline void draw_pixel(gfx_context_t *ctx, uint_16 X, uint_16 Y, bbp_t color)
{
    GFX(ctx, X, Y) = color;
    /* GFXR(ctx, X, Y) = color; */
}
void draw_sprite_alpha(gfx_context_t *ctx,
                       const sprite_t *sprite,
                       int_32 x,
                       int_32 y,
                       int_32 opacity)
{
    return;
}

void draw_sprite(gfx_context_t *ctx, const sprite_t *sprite, int_32 x, int_32 y)
{
    int_32 _left = MAX(x, 0);
    int_32 _top = MAX(y, 0);
    int_32 _right = MIN(x + sprite->width, ctx->width - 1);
    int_32 _bottom = MIN(y + sprite->height, ctx->height - 1);
    if (sprite->alpha == ALPHA_EMBEDDED) {
        /* Alpha embedded is the most important step. */
        for (uint_16 _y = 0; _y < sprite->height; ++_y) {
            if (y + _y < _top)
                continue;
            if (y + _y > _bottom)
                break;
            if (!_is_in_clip(ctx, y + _y))
                continue;
            for (uint_16 _x = 0; _x < sprite->width; ++_x) {
                if (x + _x < _left || x + _x > _right || y + _y < _top ||
                    y + _y > _bottom)
                    continue;
                GFX(ctx, x + _x, y + _y) = alpha_blend_rgba(
                    GFX(ctx, x + _x, y + _y), SPRITE(sprite, _x, _y));
            }
        }
    } else if (sprite->alpha == ALPHA_OPAQUE) {
        for (uint_16 _y = 0; _y < sprite->height; ++_y) {
            if (y + _y < _top)
                continue;
            if (y + _y > _bottom)
                break;
            if (!_is_in_clip(ctx, y + _y))
                continue;
            for (uint_16 _x = (x < _left) ? _left - x : 0;
                 _x < sprite->width && x + _x <= _right; ++_x) {
                GFX(ctx, x + _x, y + _y) = SPRITE(sprite, _x, _y) | 0xFF000000;
            }
        }
    }
}

/**
 * Fill all pixels in given ctx
 *
 *****************************************************************************/
void draw_fill(gfx_context_t *ctx, uint_32 color)
{
    for (uint_16 y = 0; y < ctx->height; ++y) {
        for (uint_16 x = 0; x < ctx->width; ++x) {
            GFX(ctx, x, y) = color;
        }
    }
}

/* // Draw a line */
/* //   Adapted from Wikipedia page on Bresenham's line algorithm */
/* void draw_line(Point start, Point end, argb_t color) */
/* { */
/*     int_16 deltaX = */
/*         ABS((end.X - */
/*              start.X));  // Delta X, change in X values, positive absolute
 * value */
/*     int_16 deltaY = -ABS( */
/*         (end.Y - */
/*          start.Y));  // Delta Y, change in Y values, negative absolute value
 */
/*     int_16 signX = */
/*         (start.X < end.X) ? 1 : -1;  // Sign of X direction, moving right */
/*                                      // (positive) or left (negative) */
/*     int_16 signY = */
/*         (start.Y < end.Y) ? 1 : -1;  // Sign of Y direction, moving down */
/*                                      // (positive) or up (negative) */
/*     int_16 error = deltaX + deltaY; */
/*     int_16 errorX2; */
/*  */
/*     while (1) { */
/*         draw_pixel(start.X, start.Y, color); */
/*         if (start.X == end.X && start.Y == end.Y) */
/*             break; */
/*  */
/*         errorX2 = error * 2; */
/*         if (errorX2 >= deltaY) { */
/*             error += deltaY; */
/*             start.X += signX; */
/*         } */
/*         if (errorX2 <= deltaX) { */
/*             error += deltaX; */
/*             start.Y += signY; */
/*         } */
/*     } */
/* } */
/*  */
/* // Draw a triangle */
/* void draw_triangle(Point vertex0, Point vertex1, Point vertex2, argb_t color)
 */
/* { */
/*     // Draw lines between all 3 points */
/*     draw_line(vertex0, vertex1, color); */
/*     draw_line(vertex1, vertex2, color); */
/*     draw_line(vertex2, vertex0, color); */
/* } */
/*  */
/* // Draw a rectangle */
/* void draw_rect(Point top_left, Point bottom_right, argb_t color) */
/* { */
/*     Point temp = bottom_right; */
/*  */
/*     // Draw 4 lines, 2 horizontal parallel sides, 2 vertical parallel sides
 */
/*     // Top of rectangle */
/*     temp.Y = top_left.Y; */
/*     draw_line(top_left, temp, color); */
/*  */
/*     // Right side */
/*     draw_line(temp, bottom_right, color); */
/*  */
/*     // Bottom */
/*     temp.X = top_left.X; */
/*     temp.Y = bottom_right.Y; */
/*     draw_line(bottom_right, temp, color); */
/*  */
/*     // Left side */
/*     draw_line(temp, top_left, color); */
/* } */
/*  */
/* // Draw a polygon */
/* void draw_polygon(Point vertex_array[], uint_8 num_vertices, argb_t color) */
/* { */
/*     // Draw lines up to last line */
/*     for (uint_8 i = 0; i < num_vertices - 1; i++) */
/*         draw_line(vertex_array[i], vertex_array[i + 1], color); */
/*  */
/*     // Draw last line */
/*     draw_line(vertex_array[num_vertices - 1], vertex_array[0], color); */
/* } */
/*  */
/* // Draw a circle */
/* //  Adapted from "Computer Graphics Principles and Practice in C 2nd Edition"
 */
/* void draw_circle(Point center, uint_16 radius, argb_t color) */
/* { */
/*     int_16 x = 0; */
/*     int_16 y = radius; */
/*     int_16 p = 1 - radius; */
/*  */
/*     // Draw initial 8 octant points */
/*     draw_pixel(center.X + x, center.Y + y, color); */
/*     draw_pixel(center.X - x, center.Y + y, color); */
/*     draw_pixel(center.X + x, center.Y - y, color); */
/*     draw_pixel(center.X - x, center.Y - y, color); */
/*     draw_pixel(center.X + y, center.Y + x, color); */
/*     draw_pixel(center.X - y, center.Y + x, color); */
/*     draw_pixel(center.X + y, center.Y - x, color); */
/*     draw_pixel(center.X - y, center.Y - x, color); */
/*  */
/*     while (x < y) { */
/*         x++; */
/*         if (p < 0) */
/*             p += 2 * x + 1; */
/*         else { */
/*             y--; */
/*             p += 2 * (x - y) + 1; */
/*         } */
/*  */
/*         // Draw next set of 8 octant points */
/*         draw_pixel(center.X + x, center.Y + y, color); */
/*         draw_pixel(center.X - x, center.Y + y, color); */
/*         draw_pixel(center.X + x, center.Y - y, color); */
/*         draw_pixel(center.X - x, center.Y - y, color); */
/*         draw_pixel(center.X + y, center.Y + x, color); */
/*         draw_pixel(center.X - y, center.Y + x, color); */
/*         draw_pixel(center.X + y, center.Y - x, color); */
/*         draw_pixel(center.X - y, center.Y - x, color); */
/*     } */
/* } */
/*  */
/* // Draw an ellipse */
/* // TODO: Put this back in later, using non-floating point values/ROUND if */
/* // possible */
/* //  Adapted from "Computer Graphics Principles and Practice in C 2nd Edition"
 */
/* // void draw_ellipse(Point center, uint_16 radiusX, uint_16 radiusY, uint_32
 */
/* // color) */
/* //{ */
/* //    int_32 rx2 = radiusX * radiusX; */
/* //    int_32 ry2 = radiusY * radiusY; */
/* //    int_32 twoRx2 = 2*rx2; */
/* //    int_32 twoRy2 = 2*ry2; */
/* //    int_32 p; */
/* //    int_32 x = 0; */
/* //    int_32 y = radiusY; */
/* //    int_32 px = 0; */
/* //    int_32 py = twoRx2 * y; */
/* // */
/* //    // Draw initial 4 quadrant points */
/* //    draw_pixel(center.X + x, center.Y + y, color); */
/* //    draw_pixel(center.X - x, center.Y + y, color); */
/* //    draw_pixel(center.X + x, center.Y - y, color); */
/* //    draw_pixel(center.X - x, center.Y - y, color); */
/* // */
/* //    // Region 1 */
/* //    p = ROUND(ry2 - (rx2 * radiusY) + (0.25 * rx2)); */
/* //    while (px < py) { */
/* //        x++; */
/* //        px += twoRy2; */
/* //        if (p < 0) p += ry2 + px; */
/* //        else { */
/* //            y--; */
/* //            py -= twoRx2; */
/* //            p += ry2 + px - py; */
/* //        } */
/* // */
/* //        // Draw 4 quadrant points */
/* //        draw_pixel(center.X + x, center.Y + y, color); */
/* //        draw_pixel(center.X - x, center.Y + y, color); */
/* //        draw_pixel(center.X + x, center.Y - y, color); */
/* //        draw_pixel(center.X - x, center.Y - y, color); */
/* //    } */
/* // */
/* //    // Region 2 */
/* //    p = ROUND(ry2 * (x + 0.5)*(x + 0.5) + rx2 * (y - 1) * (y - 1) -
 * rx2*ry2); */
/* //    while (y > 0) { */
/* //        y--; */
/* //        py -= twoRx2; if (p > 0) p += rx2 - py; */
/* //        else { */
/* //            x++; */
/* //            px += twoRy2; */
/* //            p += rx2 - py + px; */
/* //        } */
/* // */
/* //        // Draw 4 quadrant points */
/* //        draw_pixel(center.X + x, center.Y + y, color); */
/* //        draw_pixel(center.X - x, center.Y + y, color); */
/* //        draw_pixel(center.X + x, center.Y - y, color); */
/* //        draw_pixel(center.X - x, center.Y - y, color); */
/* //    } */
/* //} */
/*  */
/* // Fill an area with a solid color */
/* void boundary_fill(uint_16 X, */
/*                    uint_16 Y, */
/*                    argb_t fill_color, */
/*                    argb_t boundary_color) */
/* { */
/*     // Recursive - may use a lot of stack space */
/*     uint_8 *framebuffer = (uint_8 *) gfx_mode.physical_base_pointer; */
/*     uint_8 bytes_per_pixel = */
/*         (gfx_mode.bits_per_pixel + 1) / */
/*         8;  // Get # of bytes per pixel, add 1 to fix 15bpp modes */
/*     uint_8 draw = 0; */
/*  */
/*     framebuffer += (Y * gfx_mode.x_resolution + X) * bytes_per_pixel; */
/*  */
/*     for (uint_8 temp = 0; temp < bytes_per_pixel; temp++) { */
/*         if ((framebuffer[temp] != */
/*              (uint_8) (convert_argb(fill_color) >> (temp * 8))) && */
/*             (framebuffer[temp] != */
/*              (uint_8) (convert_argb(boundary_color) >> (temp * 8)))) { */
/*             draw = 1; */
/*             break; */
/*         } */
/*     } */
/*  */
/*     if (draw) { */
/*         for (uint_8 temp = 0; temp < bytes_per_pixel; temp++) */
/*             framebuffer[temp] = (uint_8) (convert_argb(fill_color) >> temp *
 * 8); */
/*  */
/*         // Check 4 pixels around current pixel */
/*         boundary_fill(X + 1, Y, fill_color, boundary_color); */
/*         boundary_fill(X - 1, Y, fill_color, boundary_color); */
/*         boundary_fill(X, Y + 1, fill_color, boundary_color); */
/*         boundary_fill(X, Y - 1, fill_color, boundary_color); */
/*     } */
/* } */
/*  */
/*  */
/* // Fill triangle with a solid color */
/* void fill_triangle_solid(Point p0, Point p1, Point p2, argb_t color) */
/* { */
/*     Point temp; */
/*  */
/*     // Get center (Centroid) of a triangle */
/*     temp.X = (p0.X + p1.X + p2.X) / 3; */
/*     temp.Y = (p0.Y + p1.Y + p2.Y) / 3; */
/*  */
/*     // First draw triangles sides (boundaries) */
/*     draw_triangle(p0, p1, p2, color - 1); */
/*  */
/*     // Then fill in boundaries */
/*     boundary_fill(temp.X, temp.Y, color, color - 1); */
/*  */
/*     // Then redraw boundaries to correct color */
/*     draw_triangle(p0, p1, p2, color); */
/* } */
/*  */

// Fill rectangle with a solid color
void fill_rect_solid(gfx_context_t *ctx,
                     point_t top_left,
                     point_t bottom_right,
                     argb_t color)
{
    // Brute force method
    /* bbp_t bbp_c  = convert_argb(color); */
    for (uint_16 y = top_left.Y; y < bottom_right.Y; y++)
        for (uint_16 x = top_left.X; x < bottom_right.X; x++) {
            /* argb_t _c = alpha_blend_rgba(GFX(ctx, x, y), color); */
            /* draw_pixel(ctx, x, y, _c); */
            /* bbp_t bbp_c = convert_argb(_c); */
            /* draw_pixel(ctx, x, y, bbp_c); */
            draw_pixel(ctx, x, y, color);
        }
}

void draw_rect_solid(gfx_context_t *ctx, rect_t rect, argb_t color)
{
    for (uint_16 y = rect.y; y < rect.y + rect.height; y++) {
        for (uint_16 x = rect.x; x < rect.x + rect.width; x++) {
            GFX(ctx, x, y) = color;
        }
    }
}

/* // Fill a polygon with a solid color */
/* void fill_polygon_solid(Point vertex_array[], uint_8 num_vertices, argb_t
 * color) */
/* { */
/*     Point temp; */
/*  */
/*     // Assuming this works in general, get center (Centroid) of a triangle in
 */
/*     // the polygon */
/*     temp.X = (vertex_array[0].X + vertex_array[1].X + vertex_array[2].X) / 3;
 */
/*     temp.Y = (vertex_array[0].Y + vertex_array[1].Y + vertex_array[2].Y) / 3;
 */
/*  */
/*     // First draw polygon sides (boundaries) */
/*     draw_polygon(vertex_array, num_vertices, color - 1); */
/*  */
/*     // Then fill in boundaries */
/*     boundary_fill(temp.X, temp.Y, color, color - 1); */
/*  */
/*     // Then redraw boundaries to correct color */
/*     draw_polygon(vertex_array, num_vertices, color); */
/* } */
/*  */
/* // Fill a circle with a solid color */
/* void fill_circle_solid(Point center, uint_16 radius, argb_t color) */
/* { */
/*     // Brute force method */
/*     for (int_16 y = -radius; y <= radius; y++) */
/*         for (int_16 x = -radius; x <= radius; x++) */
/*             if (x * x + y * y < radius * radius - radius) */
/*                 draw_pixel(center.X + x, center.Y + y, color); */
/* } */

// Fill an ellipse with a solid color
// TODO: Put this back in later, using non-floating point values/ROUND if
// possible
// void fill_ellipse_solid(Point center, uint_16 radiusX, uint_16 radiusY,
// argb_t color)
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

argb_t fetch_color(gfx_context_t *ctx, uint_32 X, uint_32 Y)
{
    return (argb_t) GFX(ctx, X, Y);
}

void clear_screen(gfx_context_t *ctx, argb_t color)
{
    // Get 32bit pointer to framebuffer value in VBE mode info block,
    //   dereference to get the 32bit value,
    //   get 32bit pointer to that value - memory address of framebuffer
    bbp_t set_color = convert_argb(color);
    for (uint_32 x_idx = 0; x_idx < ctx->width; x_idx++) {
        for (uint_32 y_idx = 0; y_idx < ctx->height; y_idx++) {
            GFX(ctx, x_idx, y_idx) = set_color;
        }
    }
}

// y == 46
#include <string.h>
void draw_2d_gfx_label(gfx_context_t *ctx,
                       uint_32 x,
                       uint_32 y,
                       uint_32 width,
                       uint_32 length,
                       argb_t bgcolor,
                       argb_t font_color,
                       char *label_name)
{
    point_t tl = {.X = x, .Y = y};
    point_t br = {.X = x + width, .Y = y + length};
    fill_rect_solid(ctx, tl, br, bgcolor);
    draw_2d_gfx_string(ctx, 8, x, y, font_color, label_name,
                       strlen(label_name));
}
