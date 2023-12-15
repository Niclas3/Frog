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
struct STR_DATA {
    char *str;
    uint_32 len;
    uint_32 written;
};

typedef void (*print_fn)(void *userData, char);

#define PUT(c)                 \
    do {                       \
        put_fn(userData, (c)); \
        written++;             \
    } while (0)

static uint_32 lvasprintf(void (*put_fn)(void *, char),
                          void *userData,
                          char *str,
                          const char *fmt,
                          va_list ap)
{
    char *s;
    int d;
    char c;
    char *res = str;
    int res_len = 0;
    uint_32 written = 0;
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
                while (*s) {
                    PUT(*s);
                    s++;
                }
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
                char *p_tmp = tmp;
                int tlen = itoa(d, tmp, 10);
                while (*p_tmp) {
                    PUT(*p_tmp);
                    p_tmp++;
                }
                res_len += tlen;
                break;
            case 'x':
                d = va_arg(ap, int);
                char xtmp[10] = {0};
                char *p_xtmp = xtmp;
                int xlen = itoa(d, xtmp, 16);
                while (*p_xtmp) {
                    PUT(*p_xtmp);
                    p_xtmp++;
                }
                res_len += xlen;
                break;
            case 'c':
                c = (char) va_arg(ap, int);
                PUT(c);
                res_len++;
                break;
            default: {
                char fmt_c = (char) *fmt;
                PUT(fmt_c);
                res_len++;
                break;
            }
            }
            seen_mark = false;
        } else {
            char fmt_c = (char) *fmt;
            PUT(fmt_c);
            res_len++;
        }
        fmt++;
    }
    return res_len;
}

static void print_to_stdout(void *data, char c)
{
    char buf[1] = {c};
    write(1, buf, 1);
}

static void print_to_buf(void *data, char c)
{
    struct STR_DATA *user_data = data;
    if (user_data->written < user_data->len) {
        uint_32 idx = user_data->written;
        user_data->str[idx] = c;
        user_data->written++;
    } else {
    }
}

static uint_32 lprintf(print_fn callback, const char *fmt, ...)
{
    va_list args;
    uint_32 len = 0;
    char *str;
    va_start(args, fmt);
    len = lvasprintf(callback, NULL, str, fmt, args);
    va_end(args);
    return len;
}

static uint_32 lsprintf(print_fn callback, char *str, const char *fmt, ...)
{
    va_list args;
    uint_32 len = 0;
    struct STR_DATA user_data = {
        .str = str,
        .len = 1024
    };
    va_start(args, fmt);
    len = lvasprintf(callback, &user_data, str, fmt, args);
    va_end(args);
    return len;
}

uint_32 vsprintf(char *str, const char *fmt, va_list ap)
{
    uint_32 len = 0;
    struct STR_DATA user_data = {
        .str = str,
        .len = 1024
    };
    len = lvasprintf(print_to_buf, &user_data, str, fmt, ap);
    return len;
}


uint_32 printf(const char *fmt, ...)
{
    va_list args;
    uint_32 len = 0;
    char *str;
    va_start(args, fmt);
    len = lvasprintf(print_to_stdout, NULL, str, fmt, args);
    va_end(args);
    return len;
}

// FIXME:
// get rid of mvsprintf()
static uint_32 mvsprintf(char *str, const char *fmt, va_list ap)
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
    len = mvsprintf(str, fmt, args);
    va_end(args);
    return len;
}
