#ifndef _FROG_KERNEL_PACKAGEFS_H
#define _FROG_KERNEL_PACKAGEFS_H

int packagefs_init(void);
#if defined(CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE) || \
    defined(CONFIG_FROG_TEST_DESKTOP_SOAK)
int packagefs_lifecycle_test_command(uint_32 command);
#endif

#endif
