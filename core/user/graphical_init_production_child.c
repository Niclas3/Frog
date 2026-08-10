#include <frog/graphical_startup.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

#if !defined(FROG_GRAPHICAL_CHILD_COMPOSITOR) && \
    !defined(FROG_GRAPHICAL_CHILD_DESKTOP)
#error A focused graphical child role is required
#endif

static bool string_equal(const char *left, const char *right)
{
        if (left == NULL || right == NULL)
                return false;
        while (*left != '\0' && *left == *right) {
                left++;
                right++;
        }
        return *left == *right;
}

static int_32 report(uint_32 id, bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT), "b"(id), "c"(passed)
                         : "memory");
        return result;
}

int main(uint_32 argc, const char *const argv[])
{
#ifdef FROG_GRAPHICAL_CHILD_COMPOSITOR
        bool arguments = argc == 1 && argv != NULL && argv[0] != NULL &&
                         argv[1] == NULL &&
                         string_equal(argv[0], FROG_GRAPHICAL_COMPOSITOR_PATH);

        if (report(FROG_TEST_GRAPHICAL_COMPOSITOR_IDENTITY,
                   arguments && getpid() > 1) != 0)
                return 1;
        for (uint_32 attempt = 0; attempt < 1000U; attempt++) {
                int_32 ready = report(FROG_TEST_GRAPHICAL_COMPOSITOR_RELEASE,
                                      true);

                if (ready == 1)
                        return 0;
                if (ready != 0 || wait2(NULL, 0, 1) != 0)
                        return 1;
        }
        return 1;
#else
        bool arguments = argc == 1 && argv != NULL && argv[0] != NULL &&
                         argv[1] == NULL &&
                         string_equal(argv[0], FROG_GRAPHICAL_DESKTOP_PATH);

        if (report(FROG_TEST_GRAPHICAL_DESKTOP_IDENTITY,
                   arguments && getpid() > 1) != 0)
                return FROG_DESKTOP_EXIT_RUNTIME_FAILURE;
        return FROG_DESKTOP_EXIT_COMPOSITOR_HUP;
#endif
}
