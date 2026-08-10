#ifndef _KERNEL_DESKTOP_PRODUCTION_TEST_H
#define _KERNEL_DESKTOP_PRODUCTION_TEST_H

#if defined(CONFIG_QEMU_TEST) && defined(CONFIG_FROG_TEST_DESKTOP)

#include <frog/types.h>

void desktop_production_test_observe_fork(pid_t child_pid);
int desktop_production_test_validate_exec(uint_32 id, int passed);
int desktop_production_test_validate_liveness(void);

#endif

#endif
