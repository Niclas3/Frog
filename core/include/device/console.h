#ifndef __DEVICE_CONSOLE_C
#define __DEVICE_CONSOLE_C
#include <ostype.h>

struct file;
struct partition;
struct dir;

typedef struct gfx_2d_context gfx_context_t;

#define IO_CONSOLE_REINIT 0x6001
#define IO_CONSOLE_SET    0x6002

void console_init(gfx_context_t *ctx);
void console_write(void *buf, uint_32 len);
int_32 ioctl_console(struct file *file, unsigned long request, void *argp);

int_32 console_create(struct partition *part,
                      struct dir *parent_d,
                      char *name,
                      void *target);

void console_put_char(uint_8 c);

void console_put_hex(int_32 num);

void console_put_str(char *str);

#endif
