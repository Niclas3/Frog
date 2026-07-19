#ifndef _KERNEL_MM_TEST_H
#define _KERNEL_MM_TEST_H

#include <frog/types.h>

int mm_regression_test(void);
void mm_uaccess_process_regression(void);
int_32 mm_vm_process_prepare(void);
int_32 mm_vm_process_verify_cleanup(void);

#endif
