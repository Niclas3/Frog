#ifndef _KERNEL_MM_TEST_H
#define _KERNEL_MM_TEST_H

#include <frog/types.h>

int mm_regression_test(void);
void mm_uaccess_process_regression(void);
int_32 mm_vm_process_prepare(void);
int_32 mm_vm_process_verify_cleanup(void);
int_32 mm_vm_process_arm_fork_failure(uint_32 step);
int_32 mm_vm_process_verify_refs(uint_32 expected);
int_32 mm_anon_mmap_test_command(uint_32 command);

#endif
