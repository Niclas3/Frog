#pragma once
#include <frog/types.h>

int_32 sys_execv(const char *path, const char *argv[]);

#ifdef CONFIG_QEMU_TEST
int_32 exec_test_arm_fail_before_commit(void);
#endif
