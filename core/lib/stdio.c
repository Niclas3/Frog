#include <oslib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

uint_32 vsprintf(char *str, const char *fmt, va_list ap)
{
    char *s;
    int d;
    char c;
    char *res = str;
    bool seen_mark = false;
    while (*fmt) {
        if (*fmt == '%' && !seen_mark) {
            seen_mark = true;
            fmt++;
            continue;
        }
        if (seen_mark) {
            switch (*fmt) {
            case 's':
                s = va_arg(ap, char *);
                int len = strlen(s);
                strncpy(res, s, len);
                res += len;
                break;
            case 'd':
                // TODO: add feature
                // /^\.[0-9]*d$/gm
                d = va_arg(ap, int);
                /* The number 2,147,483,647 (or hexadecimal 7FFFFFFF16) is the
                 * maximum positive value for a 32-bit signed binary integer in
                 * computing. */
                // so i choose 10 to tmp string
                char tmp[10] = {0};
                int tlen = itoa(d, tmp, 10);
                strncpy(res, tmp, tlen);
                res += tlen;
                break;
            case 'x':
                d = va_arg(ap, int);
                char xtmp[10] = {0};
                int xlen = itoa(d, xtmp, 16);
                strncpy(res, xtmp, xlen);
                res += xlen;
                break;
            case 'c':
                c = (char) va_arg(ap, int);
                *res++ = c;
                break;
            default:
                *res++ = *fmt;
                break;
            }
            seen_mark = false;
        } else {
            *res++ = *fmt;
        }
        fmt++;
    }
    *res = '\0';
    int res_len = strlen(str);
    return res_len;
}

uint_32 sprintf(char *str, const char *fmt, ...)
{
    va_list args;
    uint_32 len;
    va_start(args, fmt);
    len = vsprintf(str, fmt, args);
    va_end(args);
    return len;
}

uint_32 printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[1024] = {0};
    vsprintf(buf, fmt, args);
    va_end(args);
    return write(1, buf, strlen(buf));
}
