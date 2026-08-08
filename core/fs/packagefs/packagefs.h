#ifndef _FROG_KERNEL_PACKAGEFS_H
#define _FROG_KERNEL_PACKAGEFS_H

int packagefs_init(void);
#ifdef CONFIG_FROG_TEST_PACKAGEFS_LIFECYCLE
int packagefs_lifecycle_test_command(uint_32 command);
#endif

#endif
