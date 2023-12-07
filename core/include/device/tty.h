#ifndef __DEVICE_TTY_C
#define __DEVICE_TTY_C
#include <ostype.h>

void tty_init(void);

void tty_put_char(uint_8 c);

void tty_put_hex(int_32 num);

void tty_put_str(char *str);

#endif
