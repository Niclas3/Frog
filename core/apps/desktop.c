#include <frog/types.h>

#define DESKTOP_EXEC_SMOKE_STATUS 43

static volatile uint_32 desktop_data_cookie = 0x44534b31U;
static volatile uint_32 desktop_bss_cookie;

static bool string_equal(const char *left, const char *right)
{
        if (!left || !right)
                return false;
        while (*left && *right) {
                if (*left++ != *right++)
                        return false;
        }
        return *left == *right;
}

static bool exec_smoke_requested(int argc, char **argv)
{
        return argc == 2 && argv && argv[0] &&
               string_equal(argv[1], "--exec-smoke");
}

int main(int argc, char **argv)
{
        if (exec_smoke_requested(argc, argv)) {
                return desktop_data_cookie == 0x44534b31U &&
                       desktop_bss_cookie == 0
                           ? DESKTOP_EXEC_SMOKE_STATUS
                           : 1;
        }

        /* Task 13 adds the Poudland protocol lifecycle. */
        return 0;
}
