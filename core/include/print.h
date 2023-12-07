#ifndef __LIB_PRINT_H
#define __LIB_PRINT_H
#include <ostype.h>
void put_char(uint_8 char_asci);
void put_str(char* message);
void put_int(uint_32 num);
void set_cursor(uint_32 cursor_pos);
void cls_screen(void);
#endif
