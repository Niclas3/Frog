#include <kernel_print.h>
#include <oslib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

uint_32 kprint(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {0};
    vsprintf(buf, fmt, args);
    va_end(args);
    return write(1, buf, strlen(buf));
}
