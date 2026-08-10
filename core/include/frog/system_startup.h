#ifndef _FROG_SYSTEM_STARTUP_H
#define _FROG_SYSTEM_STARTUP_H

#include <frog/types.h>

#define FROG_SYSTEM_INIT_CONFIG_PATH "/etc/frog/init.conf"
#define FROG_SYSTEM_INIT_GRAPHICAL_PATH "/sbin/init-graphical"
#define FROG_SYSTEM_INIT_CONFIG_MAX 64U

enum frog_system_init_status {
        FROG_SYSTEM_INIT_OK = 0,
        FROG_SYSTEM_INIT_MALFORMED = 90,
        FROG_SYSTEM_INIT_UNSUPPORTED_MODE = 91,
        FROG_SYSTEM_INIT_OPEN_FAILED = 92,
        FROG_SYSTEM_INIT_READ_FAILED = 93,
        FROG_SYSTEM_INIT_CLOSE_FAILED = 94,
        FROG_SYSTEM_INIT_EXEC_FAILED = 95,
        FROG_SYSTEM_INIT_EXEC_RETURNED = 96,
};

struct frog_system_init_ops {
        int_32 (*open)(const char *path);
        int_32 (*read)(int_32 fd, void *buffer, uint_32 length);
        int_32 (*close)(int_32 fd);
        int_32 (*exec)(const char *path, const char *const argv[]);
};

int frog_system_init_parse_config(const void *data, uint_32 length);
int frog_system_init_run(const struct frog_system_init_ops *ops);
void frog_system_init_report_status(
    int status, void (*emit)(char byte, void *context), void *context);

#endif
