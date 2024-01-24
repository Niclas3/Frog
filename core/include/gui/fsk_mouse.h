#ifndef __GUI_FSK_MOUSE_H
#define __GUI_FSK_MOUSE_H
#include <ostype.h>
#include <gua/2d_graphics.h>

struct fsk_mouse{
    gfx_context_t *ctx;
    Point point;
};

void create_fsk_mouse(gfx_context_t *ctx, uint_32 cursor_x, uint_32 cursor_y);

#endif
