#ifndef __DEVICE_CONSOLE_C
#define __DEVICE_CONSOLE_C
#include <ostype.h>


typedef struct gfx_2d_context gfx_context_t;

void console_init(gfx_context_t *ctx);
void console_write(void *buf, uint_32 len);

void console_put_char(uint_8 c);

void console_put_hex(int_32 num);

void console_put_str(char *str);

#endif
