#ifndef __KERNEL_DISK_INIT_LOADER_H
#define __KERNEL_DISK_INIT_LOADER_H

#include <frog/types.h>

/*
 * Create Frog's one initial user process from an ELF visible in the current
 * VFS namespace. path and argv are trusted kernel-owned strings, not user
 * pointers. On success the process is published as numeric PID 1 and
 * *pid_out is set to 1. On failure no user process is published and
 * *pid_out remains -1.
 */
int_32 process_execute_init_path(const char *path,
                                 const char *const argv[],
                                 pid_t *pid_out);

#ifdef CONFIG_FROG_TEST_DISK_INIT_LOADER
enum disk_init_loader_test_failure {
        DISK_INIT_LOADER_FAIL_ARGUMENT_ALLOCATION = 1,
        DISK_INIT_LOADER_FAIL_IMAGE_MAPPING,
        DISK_INIT_LOADER_FAIL_AFTER_PID_SWAP,
};

int_32 disk_init_loader_test_fail_once(
    enum disk_init_loader_test_failure failure);
bool disk_init_loader_test_take_failure(
    enum disk_init_loader_test_failure failure);
void disk_init_loader_regression_run(void);
#endif

#endif
