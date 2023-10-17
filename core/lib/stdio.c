#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <oslib.h>

uint_32 sprintf(char *str, const char *fmt, ...)
{
    va_list ap;
    char *s;
    int d;
    char c;
    char *res = str;
    bool seen_mark = false;
    va_start(ap, fmt);
    while (*fmt) {
        if(*fmt == '%' && !seen_mark){
            seen_mark = true;
            fmt++;
            continue;
        }
        if(seen_mark){
            switch (*fmt) {
            case 's':
                s = va_arg(ap, char *);
                int len = strlen(s);
                strcpy(res, s);
                res += len;
                break;
            case 'd':
                d = va_arg(ap, int);
/* The number 2,147,483,647 (or hexadecimal 7FFFFFFF16) is the maximum positive value for a 32-bit signed binary integer in computing. */
// so i choose 10 to tmp string
                char tmp[10];
                int tlen = itoa(d, tmp, 10);
                strcpy(res, tmp);
                res += tlen;
                break;
            case 'x':
                d = va_arg(ap, int);
                char xtmp[10];
                int xlen = itoa(d, xtmp, 16);
                strcpy(res, xtmp);
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
    va_end(ap);
    *res = '\0';
    int res_len = strlen(str);
    return res_len;
}

