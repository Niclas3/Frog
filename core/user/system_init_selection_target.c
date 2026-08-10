#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>

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

static bool bytes_equal(const char *left, const char *right, uint_32 length)
{
        for (uint_32 index = 0; index < length; index++) {
                if (left[index] != right[index])
                        return false;
        }
        return true;
}

static void report(uint_32 id, bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT), "b"(id), "c"(passed)
                         : "memory");
        (void) result;
}

static bool root_is_usable(void)
{
        static const char expected[] = "mode=graphical\n";
        char config[sizeof(expected)];
        int_32 fd = open("/etc/frog/init.conf", O_RDONLY);
        int_32 count;
        bool passed;

        if (fd < 0)
                return false;
        count = read(fd, config, sizeof(config));
        passed = count == (int_32) (sizeof(expected) - 1U) &&
                 bytes_equal(config, expected, sizeof(expected) - 1U) &&
                 read(fd, config, 1) == 0;
        return close(fd) == 0 && passed;
}

static bool devfs_is_usable(void)
{
        char event;
        int_32 fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
        bool passed;

        if (fd < 0)
                return false;
        passed = read(fd, &event, 1) == -EAGAIN;
        return close(fd) == 0 && passed;
}

static bool packagefs_is_usable(void)
{
        static const char path[] = "/dev/pkg/system-init-selection";
        int_32 server = open(path, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC |
                                      O_NONBLOCK);
        int_32 client = server >= 0 ?
            open(path, O_RDWR | O_CLOEXEC | O_NONBLOCK) : -1;
        bool passed = server >= 0 && client >= 0;
        bool cleanup = true;

        if (client >= 0)
                cleanup = close(client) == 0 && cleanup;
        if (server >= 0)
                cleanup = close(server) == 0 && cleanup;
        return passed && cleanup;
}

int main(uint_32 argc, const char *const argv[])
{
        bool arguments = argc == 1 && argv != NULL && argv[0] != NULL &&
                         argv[1] == NULL &&
                         string_equal(argv[0], "/sbin/init-graphical");

        report(FROG_TEST_SYSTEM_INIT_SELECTION_IDENTITY,
               arguments && getpid() == 1);
        report(FROG_TEST_SYSTEM_INIT_SELECTION_ROOT, root_is_usable());
        report(FROG_TEST_SYSTEM_INIT_SELECTION_DEVFS, devfs_is_usable());
        report(FROG_TEST_SYSTEM_INIT_SELECTION_PACKAGEFS,
               packagefs_is_usable());
        testsyscall(0);
        for (;;)
                (void) wait2(NULL, 0, 1000);
}
