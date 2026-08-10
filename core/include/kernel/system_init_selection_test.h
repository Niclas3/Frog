#ifndef _KERNEL_SYSTEM_INIT_SELECTION_TEST_H
#define _KERNEL_SYSTEM_INIT_SELECTION_TEST_H

#if defined(CONFIG_QEMU_TEST) && \
    defined(CONFIG_FROG_TEST_SYSTEM_INIT_SELECTION)

#include <frog/types.h>

void system_init_selection_test_observe_output(char byte);
void system_init_selection_test_observe_wait(const void *user_fds,
                                             uint_32 count,
                                             int_32 timeout_ms);
bool system_init_selection_test_identity_ok(void);

#endif

#endif
