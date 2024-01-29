// Window compositor

// watch mouse and keyboard event
#include <gua/poudland.h>
#include <hid/mouse.h>
#include <ostype.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

/**
 * Create a graphical context around a poudland window.
 */
gfx_context_t *init_graphics_poudland(poudland_window_t *window)
{
    gfx_context_t *out = malloc(sizeof(gfx_context_t));
    out->width = window->width;
    out->height = window->height;
    out->stride = window->width * sizeof(uint_32);
    out->depth = 32;
    out->size = GFX_H(out) * GFX_W(out) * GFX_D(out);
    out->buffer = window->buffer;
    out->backbuffer = out->buffer;
    out->clips = NULL;
    return out;
}

gfx_context_t *init_graphics_poudland_double_buffer(poudland_window_t *window)
{
    gfx_context_t *out = init_graphics_poudland(window);
    out->backbuffer = malloc(GFX_D(out) * GFX_W(out) * GFX_H(out));
    return out;
}

