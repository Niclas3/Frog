#pragma once

#include <ostype.h>
#include <sys/FSK_Color.h>
#include <math.h>

#define GFX_W(ctx) ((ctx)->width)     /* Display width */
#define GFX_H(ctx) ((ctx)->height)    /* Display height */
#define GFX_D(ctx) ((ctx)->depth / 8) /* Display byte depth */
#define GFX_S(ctx) ((ctx)->stride)    /* Stride */
#define SPRITE(sprite,x,y) sprite->bitmap[sprite->width * (y) + (x)]
#define SMASKS(sprite,x,y) sprite->masks[sprite->width * (y) + (x)]

#define ALPHA_OPAQUE   0
#define ALPHA_MASK     1
#define ALPHA_EMBEDDED 2
#define ALPHA_INDEXED  3
#define ALPHA_FORCE_SLOW_EMBEDDED 4

typedef struct sprite {
	uint_16 width;
	uint_16 height;
	uint_32 * bitmap;
	uint_32 * masks;
	uint_32 blank;
	uint_8  alpha;
} sprite_t;

#define _A(color) ((color & 0xFF000000) >> 0x18)
#define _R(color) ((color & 0x00FF0000) >> 0x10)
#define _G(color) ((color & 0x0000FF00) >> 0x8)
#define _B(color) ((color & 0x000000FF) >> 0x0)

#define GFX(ctx,x,y) *((uint_32 *)&((ctx)->backbuffer)[(GFX_S(ctx) * (y) + (x) * GFX_D(ctx))])
#define GFXR(ctx,x,y) *((uint_32 *)&((ctx)->buffer)[(GFX_S(ctx) * (y) + (x) * GFX_D(ctx))])

// gfx context
typedef struct gfx_2d_context {
    uint_32 width;
    uint_32 height;
    uint_32 depth;  // color depth aka bits_per_pixel
    uint_32 size;
    char *buffer;
    char *backbuffer;  // ready for double buffer
    uint_32 stride;
    char *clips;       // damage region
    int_32 clips_size;
} gfx_context_t;

typedef uint_32 argb_t;
typedef uint_32 bbp_t;

typedef struct {
    int_32 X;
    int_32 Y;
} Point;

typedef struct {
    int_32 x;
    int_32 y;
    uint_32 width;
    uint_32 height;
} rect_t;

// P1 is less than P2
#define TWO_POINTS_TO_RECT(A, B, P1, P2) \
        {                                \
            .x      = (A),               \
            .y      = (B),               \
            .width  = ABS((P1).X - (P2).X),   \
            .height = ABS((P1).Y - (P2).Y)    \
        }

gfx_context_t *init_gfx_fullscreen(void);
gfx_context_t *init_gfx_fullscreen_double_buffer(void);

sprite_t *create_sprite(uint_32 width, uint_32 height, int_32 alpha);
void sprite_free(sprite_t *sprite);

void gfx_add_clip(gfx_context_t *ctx, int_32 x, int_32 y, int_32 w, int_32 h);
void gfx_clear_clip(gfx_context_t *ctx);
void gfx_free_clip(gfx_context_t *ctx);

void clear_buffer(gfx_context_t *ctx);
void flip(gfx_context_t *ctx);
// draw some graphic patterns
void draw_pixel(gfx_context_t *ctx, uint_16 X, uint_16 Y, bbp_t color);
void draw_sprite(gfx_context_t *ctx, const sprite_t *sprite, int_32 x, int_32 y);
void draw_sprite_alpha(gfx_context_t *ctx,
                       const sprite_t *sprite,
                       int_32 x,
                       int_32 y,
                       int_32 opacity);
void draw_fill(gfx_context_t *ctx, uint_32 color);
void fill_rect_solid(gfx_context_t *ctx,
                     Point top_left,
                     Point bottom_right,
                     argb_t color);
void draw_rect_solid(gfx_context_t *ctx, rect_t rect, argb_t color);
void clear_screen(gfx_context_t *ctx, argb_t color);

uint_32 draw_2d_gfx_asc_char(gfx_context_t *ctx,
                             int font_size,
                             int x,
                             int y,
                             argb_t color,
                             char c);

uint_32 draw_2d_gfx_string(gfx_context_t *ctx,
                           int font_size,
                           int x,
                           int y,
                           argb_t color,
                           char *str,
                           uint_32 str_len);

uint_32 draw_2d_gfx_hex(gfx_context_t *ctx,
                        int font_size,
                        int x,
                        int y,
                        argb_t color,
                        int_32 num);
uint_32 draw_2d_gfx_dec(gfx_context_t *ctx,
                        int font_size,
                        int x,
                        int y,
                        argb_t color,
                        int_32 num);

// helper function
bbp_t convert_argb(const argb_t argbcolor);
argb_t convert_bbp(const bbp_t bbpcolor);

uint_32 rgb(uint_8 r, uint_8 g, uint_8 b);
uint_32 rgba(uint_8 r, uint_8 g, uint_8 b, uint_8 a);

uint_32 alpha_blend_rgba(uint_32 bottom, uint_32 top);
argb_t fetch_color(gfx_context_t *ctx, uint_32 X, uint_32 Y);

// Components
void draw_2d_gfx_label(gfx_context_t *ctx,
                       uint_32 x,
                       uint_32 y,
                       uint_32 width,
                       uint_32 length,
                       argb_t bgcolor,
                       argb_t font_color,
                       char *label_name);
