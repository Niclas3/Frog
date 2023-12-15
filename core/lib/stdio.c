#include <oslib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

/**
 * The functions lvasprintf() are analogs of sprintf(3) and
 * vsprintf(3),take a lambda() aka a function pointer,except that they allocate
 *a string large enough to hold the output including the terminating null byte,
 *and return a pointer to it via the first argument. This pointer should be
 *passed to free(3) to release the allocated storage when it is no longer
 *needed.
 *****************************************************************************/

typedef void (*print_fn)(void *str, uint_32 len);
static uint_32 lvasprintf(void (*put_fn)(void *, uint_32),
                          char *str,
                          const char *fmt,
                          va_list ap)
{
    char *s;
    int d;
    char c;
    char *res = str;
    int res_len = 0;
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
                if (s == NULL) {
                    s = "(null)";
                }
                int len = strlen(s);
                put_fn(s, len);
                res_len += len;
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
                put_fn(tmp, tlen);
                res_len += tlen;
                break;
            case 'x':
                d = va_arg(ap, int);
                char xtmp[10] = {0};
                int xlen = itoa(d, xtmp, 16);
                put_fn(xtmp, xlen);
                res_len += xlen;
                break;
            case 'c':
                c = (char) va_arg(ap, int);
                put_fn(&c, 1);
                res_len++;
                break;
            default:
                put_fn(fmt, 1);
                res_len++;
                break;
            }
            seen_mark = false;
        } else {
            put_fn(fmt, 1);
            res_len++;
        }
        fmt++;
    }
    return res_len;
}

static void print_to_stdout(void *str, uint_32 len)
{
    write(1, str, len);
}

uint_32 lvprintf(print_fn callback, const char *fmt, ...)
{
    va_list args;
    uint_32 len;
    char *str;
    va_start(args, fmt);
    lvasprintf(callback, str, fmt, args);
    va_end(args);
    return len;
}

uint_32 printf(const char *fmt, ...)
{
    va_list args;
    uint_32 len;
    char *str;
    va_start(args, fmt);
    lvasprintf(print_to_stdout, str, fmt, args);
    va_end(args);
    return len;
}

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
                if (s == NULL) {
                    s = "(null)";
                }
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
