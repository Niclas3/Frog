#include <device/console.h>
#include <print.h>
#include <string.h>
#include <sys/2d_graphics.h>
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

static gfx_context_t *console_gfx;
void console_init(gfx_context_t *ctx)
{
    lock_init(&gl_console_lock);
    console_gfx = ctx;
}

static uint_32 base_x = 0;
static uint_32 base_y = 40;
static uint_32 margin = 0;
static uint_32 font_sz = 8;
static uint_32 line_pixels = 1000;
int_32 console_write(void *buf, uint_32 len)
{
    uint_32 rlen = draw_2d_gfx_string(console_gfx, font_sz, base_x, base_y,
                                      FSK_PINK, buf, len);
    base_x += (font_sz + margin);
    if(base_x % line_pixels == 0){
        base_y += (2*font_sz);
        base_x = 0;
    }
    flip(console_gfx);
    return rlen;
}
