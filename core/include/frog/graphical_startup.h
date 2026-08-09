#ifndef _FROG_GRAPHICAL_STARTUP_H
#define _FROG_GRAPHICAL_STARTUP_H

#include <frog/types.h>

#define FROG_DESKTOP_EXIT_CONNECT_TIMEOUT 70
#define FROG_DESKTOP_EXIT_COMPOSITOR_HUP  71
#define FROG_DESKTOP_EXIT_RUNTIME_FAILURE 72

#define FROG_GRAPHICAL_EXIT_COMPOSITOR_FORK 80
#define FROG_GRAPHICAL_EXIT_DESKTOP_FORK    81
#define FROG_GRAPHICAL_EXIT_COMPOSITOR_EXEC 82
#define FROG_GRAPHICAL_EXIT_DESKTOP_EXEC    83
#define FROG_GRAPHICAL_EXIT_COMPOSITOR_RUN  84
#define FROG_GRAPHICAL_EXIT_DESKTOP_RUN     85
#define FROG_GRAPHICAL_EXIT_WAIT            86

enum graphical_child {
        GRAPHICAL_CHILD_COMPOSITOR = 1,
        GRAPHICAL_CHILD_DESKTOP,
};

struct graphical_init_ops {
        pid_t (*spawn)(enum graphical_child child, const char *path);
        pid_t (*wait)(int_32 *status);
};

int graphical_init_supervise(const struct graphical_init_ops *ops);
void graphical_init_report_status(
    int_32 status, void (*emit)(char byte, void *context), void *context);

#endif
