#include <frog/string.h>
#include <frog/syscall.h>

extern uint_32 lvasprintf(void (*put_fn)(void *, char),
                          void *userData,
                          char *str,
                          const char *fmt,
                          va_list ap);

static void print_to_stdout(void *data, char c)
{
    char buf[1] = {c};
    write(1, buf, 1);
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
