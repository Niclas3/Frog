#include <frog/system_startup.h>

static bool bytes_equal(const void *left, const char *right, uint_32 length)
{
        const char *left_bytes = left;

        for (uint_32 index = 0; index < length; ++index) {
                if (left_bytes[index] != right[index])
                        return false;
        }
        return true;
}

int frog_system_init_parse_config(const void *data, uint_32 length)
{
        static const char graphical[] = "mode=graphical\n";
        static const char tty[] = "mode=tty\n";

        if (!data)
                return FROG_SYSTEM_INIT_MALFORMED;
        if (length == sizeof(graphical) - 1U &&
            bytes_equal(data, graphical, length))
                return FROG_SYSTEM_INIT_OK;
        if (length == sizeof(tty) - 1U && bytes_equal(data, tty, length))
                return FROG_SYSTEM_INIT_UNSUPPORTED_MODE;
        return FROG_SYSTEM_INIT_MALFORMED;
}

int frog_system_init_run(const struct frog_system_init_ops *ops)
{
        char config[FROG_SYSTEM_INIT_CONFIG_MAX];
        uint_32 used = 0;
        int_32 fd;
        int_32 result;
        bool read_failed = false;
        bool overlong = false;
        int status;
        static const char *const graphical_argv[] = {
            FROG_SYSTEM_INIT_GRAPHICAL_PATH,
            NULL,
        };

        if (!ops || !ops->open || !ops->read || !ops->close || !ops->exec)
                return FROG_SYSTEM_INIT_OPEN_FAILED;
        fd = ops->open(FROG_SYSTEM_INIT_CONFIG_PATH);
        if (fd < 0)
                return FROG_SYSTEM_INIT_OPEN_FAILED;
        for (;;) {
                if (used == sizeof(config)) {
                        char discarded[16];

                        result = ops->read(fd, discarded, sizeof(discarded));
                        if (result < 0)
                                read_failed = true;
                        else if (result > 0)
                                overlong = true;
                        if (result <= 0)
                                break;
                        continue;
                }
                result = ops->read(fd, config + used, sizeof(config) - used);
                if (result < 0) {
                        read_failed = true;
                        break;
                }
                if (result == 0)
                        break;
                used += (uint_32) result;
        }
        if (ops->close(fd) < 0)
                return read_failed ? FROG_SYSTEM_INIT_READ_FAILED :
                                     FROG_SYSTEM_INIT_CLOSE_FAILED;
        if (read_failed)
                return FROG_SYSTEM_INIT_READ_FAILED;
        if (overlong)
                return FROG_SYSTEM_INIT_MALFORMED;
        status = frog_system_init_parse_config(config, used);
        if (status != FROG_SYSTEM_INIT_OK)
                return status;
        result = ops->exec(FROG_SYSTEM_INIT_GRAPHICAL_PATH, graphical_argv);
        return result < 0 ? FROG_SYSTEM_INIT_EXEC_FAILED :
                            FROG_SYSTEM_INIT_EXEC_RETURNED;
}

void frog_system_init_report_status(
    int status, void (*emit)(char byte, void *context), void *context)
{
        char line[] = "system-init status=00\n";
        uint_32 value = status < 0 ? 99U : (uint_32) status;

        if (!emit)
                return;
        if (value > 99U)
                value = 99U;
        line[19] = (char) ('0' + value / 10U);
        line[20] = (char) ('0' + value % 10U);
        for (uint_32 index = 0; index < sizeof(line) - 1U; ++index)
                emit(line[index], context);
}

#ifndef SYSTEM_INIT_HOST_TEST
#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/syscall.h>

#if (defined(FROG_SYSTEM_INIT_TEST_OPEN_FAILURE) || \
     defined(FROG_SYSTEM_INIT_TEST_READ_FAILURE) || \
     defined(FROG_SYSTEM_INIT_TEST_CLOSE_FAILURE) || \
     defined(FROG_SYSTEM_INIT_TEST_EXEC_RETURNED)) && \
    !defined(FROG_SYSTEM_INIT_SELECTION_TEST_BUILD)
#error System Init failure injection is restricted to the focused test artifact
#endif

static int_32 runtime_open(const char *path)
{
#ifdef FROG_SYSTEM_INIT_TEST_OPEN_FAILURE
        (void) path;
        return -EIO;
#else
        return open(path, O_RDONLY);
#endif
}

static int_32 runtime_read(int_32 fd, void *buffer, uint_32 length)
{
#ifdef FROG_SYSTEM_INIT_TEST_READ_FAILURE
        (void) fd;
        (void) buffer;
        (void) length;
        return -EIO;
#else
        return read(fd, buffer, length);
#endif
}

static int_32 runtime_close(int_32 fd)
{
#ifdef FROG_SYSTEM_INIT_TEST_CLOSE_FAILURE
        int_32 result = close(fd);

        return result < 0 ? result : -EIO;
#else
        return close(fd);
#endif
}

static int_32 runtime_exec(const char *path, const char *const argv[])
{
#ifdef FROG_SYSTEM_INIT_TEST_EXEC_RETURNED
        (void) path;
        (void) argv;
        return 0;
#else
        return execv(path, (const char **) argv);
#endif
}

static void emit_console(char byte, void *context)
{
        (void) context;
        putc(byte);
}

int main(void)
{
        static const struct frog_system_init_ops ops = {
            .open = runtime_open,
            .read = runtime_read,
            .close = runtime_close,
            .exec = runtime_exec,
        };
        int status = frog_system_init_run(&ops);

        frog_system_init_report_status(status, emit_console, NULL);
        for (;;)
                (void) wait2(NULL, 0, 1000);
}
#endif
