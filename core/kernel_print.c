#include <kernel_print.h>
#include <stdarg.h>
#include <stdio.h>
#include <print.h>

uint_32 kprint(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {0};
    vsprintf(buf, fmt, args);
    va_end(args);
    put_str(buf);
    return 0;
}
