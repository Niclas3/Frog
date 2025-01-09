#include <frog/printk.h>
#include <frog/stdarg.h>
#include <stdio.h>
#include <print.h>

#define DEMSG_LEN 1024

char __ringbuf[DEMSG_LEN]; // a ring buffer for printk

int printk(const char *fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        char buf[1024] = {0};
        vsprintf(buf, fmt, args);
        va_end(args);
        put_str(buf);
        return 0;
}

int printk_with_cls(const char *fmt, ...)
{
        /* cls_screen(); */
        va_list args;
        va_start(args, fmt);
        char buf[1024] = {0};
        vsprintf(buf, fmt, args);
        va_end(args);
        /* put_str(buf); */
        return 0;
}
