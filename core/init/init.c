#include <frog/syscall.h>

#ifdef CONFIG_QEMU_TEST
#include <asm/i386_syscall_common.h>
#include <frog/errno.h>
#include <kernel/qemu_test.h>
#ifdef CONFIG_FROG_TEST_DISK
#include <kernel/fs_regression.h>
#endif
#endif

// first user process
void init(void)
{
#ifdef CONFIG_QEMU_TEST
        int invalid_result = _syscall0(SYS_NR_COUNT + 17);
        int unimplemented_result = _syscall0(SYS_STAT);
        frog_test_case("syscall.out-of-range",
                       invalid_result == -ENOSYS);
        frog_test_case("syscall.unimplemented",
                       unimplemented_result == -ENOSYS);
#ifdef CONFIG_FROG_TEST_DISK
        fs_regression_run_user();
#endif
#endif
        testsyscall(1);
        while (1) {
        }
}
