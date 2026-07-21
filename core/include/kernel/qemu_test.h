#ifndef __FROG_QEMU_TEST_H
#define __FROG_QEMU_TEST_H

void frog_test_begin(const char *profile);
void frog_test_case(const char *name, int passed);
void frog_test_milestone(const char *name, int passed);
void frog_test_sync(const char *name);
int frog_test_has_failures(void);
void frog_test_finish(void);
void frog_test_abort(const char *reason);

#endif
