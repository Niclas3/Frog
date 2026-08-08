#include <frog/syscall.h>
#include <frog/errno.h>
#include <frog/test.h>
#include <frog/types.h>

#define EXEC_TARGET_STATUS 73

static volatile uint_32 initialized_data = 0x13579bdfU;
static volatile uint_8 zero_bss[32];

static int_32 target_read(int_32 fd, void *buffer, uint_32 length)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buffer),
                           "d"(length)
                         : "memory");
        return result;
}

static bool target_string_equal(const char *left, const char *right)
{
        while (*left != '\0' && *left == *right) {
                left++;
                right++;
        }
        return *left == *right;
}

int exec_target_main(uint_32 argc, const char *const argv[])
{
        char inherited = 0;
        bool bss_zero = true;

        for (uint_32 index = 0; index < sizeof(zero_bss); index++) {
                if (zero_bss[index] != 0) {
                        bss_zero = false;
                        break;
                }
        }

        bool arguments = argc == 3 && argv != NULL && argv[0] != NULL &&
                         argv[1] != NULL && argv[2] != NULL &&
                         argv[3] == NULL &&
                         target_string_equal(argv[0], "exec-target") &&
                         target_string_equal(argv[1], "alpha") &&
                         target_string_equal(argv[2], "beta");
        bool data_ok = initialized_data == 0x13579bdfU;
        if (argc == 1 && argv != NULL && argv[0] != NULL &&
            argv[1] == NULL &&
            target_string_equal(argv[0], "cloexec-target")) {
                bool fd_closed = target_read(0, &inherited, 1) == -EBADF;

                return data_ok && bss_zero && fd_closed ?
                           FROG_TEST_PACKAGEFS_CLOEXEC_STATUS : 91;
        }
        bool fd_ok = target_read(0, &inherited, 1) == 1 && inherited == 'Q';

        return arguments && data_ok && bss_zero && fd_ok ?
                   EXEC_TARGET_STATUS : 91;
}
