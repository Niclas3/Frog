#ifndef __FROG_PANIC_H

void panic(const char *file, int line, const char *func, const char *msg);

#define PANIC(msg) \
    panic(__FILE__, __LINE__, __func__, msg)

#define PANIC_IF(cond, msg) \
    do { if (cond) PANIC(msg); } while (0)

#endif
