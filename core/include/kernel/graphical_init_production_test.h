#ifndef _KERNEL_GRAPHICAL_INIT_PRODUCTION_TEST_H
#define _KERNEL_GRAPHICAL_INIT_PRODUCTION_TEST_H

#if defined(CONFIG_QEMU_TEST) && \
    defined(CONFIG_FROG_TEST_GRAPHICAL_INIT_PRODUCTION)

#include <frog/types.h>

int_32 graphical_init_production_test_child_report(uint_32 id,
                                                   int_32 passed);
void graphical_init_production_test_observe_output(char byte);
void graphical_init_production_test_observe_wait(const void *user_fds,
                                                 uint_32 count,
                                                 int_32 timeout_ms);

#endif

#endif
