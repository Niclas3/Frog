#ifndef __DEVICE_CONSOLE_C
#define __DEVICE_CONSOLE_C
#include <ostype.h>

void console_init(void);

void console_put_char(uint_8 c);

void console_put_hex(int_32 num);

void console_put_str(char *str);

#endif
