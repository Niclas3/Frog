#include <device/console.h>
#include <fs/fs.h>
#include <print.h>
#include <stdio.h>
#include <string.h>
#include <gua/2d_graphics.h>
#include <sys/memory.h>
#include <sys/semaphore.h>

struct lock gl_console_lock;

void console_put_char(uint_8 c)
{
    lock_fetch(&gl_console_lock);
    put_char(c);
    lock_release(&gl_console_lock);
}

void console_put_hex(int_32 num)
{
    lock_fetch(&gl_console_lock);
    put_int(num);
    lock_release(&gl_console_lock);
}

void console_put_str(char *str)
{
    lock_fetch(&gl_console_lock);
    put_str(str);
    lock_release(&gl_console_lock);
}


struct enter_stack {
    uint_32 prev_line_y;
    uint_32 prev_line_x;
};

static struct enter_stack *line_stack;
static uint_32 stack_pointer;
static gfx_context_t *console_gfx;
void console_init(gfx_context_t *ctx)
{
    lock_init(&gl_console_lock);
    console_gfx = ctx;
    line_stack =
        sys_malloc(sizeof(struct enter_stack) * 100);  // 100 for 100 lines
}

static uint_32 base_x = 0;
static uint_32 base_y = 40;
static uint_32 margin = 0;
static uint_32 font_sz = 8;
static uint_32 line_pixels = 1000;
void console_write(void *buf, uint_32 len)
{
    lock_fetch(&gl_console_lock);
    char *c = buf;
    if (*c == '\b') {
        Point lt = {.X = base_x, .Y = base_y};
        Point rd = {.X = base_x + font_sz, .Y = base_y + font_sz * 2};
        gfx_add_clip(console_gfx, base_x,base_y, rd.X, rd.Y);
        fill_rect_solid(console_gfx, lt, rd, FSK_ROSY_BROWN | 0xff000000);
        if (base_x <= 0) {
            base_y -= (font_sz * 2);
            if (stack_pointer != 0) {
                stack_pointer--;
            }
            base_x = line_stack[stack_pointer].prev_line_x;
        } else {
            base_x -= font_sz;
        }
    } else if (*c == '\r' || *c == '\n') {
        line_stack[stack_pointer].prev_line_x = base_x;
        line_stack[stack_pointer].prev_line_y = base_y;
        stack_pointer++;
        base_x = 0;
        base_y += (2 * font_sz);
    } else {
        base_x += (font_sz + margin);
        if (base_x % line_pixels == 0) {
            base_y += (2 * font_sz);
            base_x = 0;
        }
        gfx_add_clip(console_gfx, base_x,base_y,font_sz ,font_sz*2);
        draw_2d_gfx_string(console_gfx, font_sz, base_x, base_y,
                                         FSK_LIGHT_GRAY, buf, len);
    }
    flip(console_gfx);
    gfx_clear_clip(console_gfx);
    gfx_free_clip(console_gfx);
    lock_release(&gl_console_lock);
}
