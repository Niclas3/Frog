#ifndef __LIB_STDIO_H
#define __LIB_STDIO_H
#include <ostype.h>
#include <stdarg.h>

uint_32 vsprintf(char *str, const char *fmt, va_list ap);
uint_32 sprintf(char* str, const char *fmt, ...);
uint_32 printf(const char *fmt, ...);
uint_32 lprintf(const char *fmt, ...);


#endif
