#ifndef __KERNEL_FS_REGRESSION_H
#define __KERNEL_FS_REGRESSION_H

const char *fs_regression_profile(void);
int fs_regression_mount_flags(void);
void fs_regression_run_kernel(int init_result,
                              int mount_result,
                              int rollback_result);
void fs_regression_run_user(void);

#endif
