#include <frog/system_startup.h>

struct fake_runtime {
        const char *input;
        uint_32 input_length;
        uint_32 offset;
        int_32 open_result;
        int_32 read_failure;
        int_32 close_result;
        int_32 exec_result;
        uint_32 open_calls;
        uint_32 close_calls;
        uint_32 exec_calls;
        char output[32];
        uint_32 output_length;
};

static struct fake_runtime runtime;

static bool equal(const char *left, const char *right)
{
        while (*left && *right) {
                if (*left++ != *right++)
                        return false;
        }
        return *left == *right;
}

static int_32 fake_open(const char *path)
{
        runtime.open_calls++;
        if (!equal(path, FROG_SYSTEM_INIT_CONFIG_PATH))
                return -99;
        return runtime.open_result;
}

static int_32 fake_read(int_32 fd, void *buffer, uint_32 length)
{
        uint_32 remaining;
        uint_32 count;
        char *destination = buffer;

        if (fd != runtime.open_result)
                return -99;
        if (runtime.read_failure)
                return runtime.read_failure;
        if (runtime.offset == runtime.input_length)
                return 0;
        remaining = runtime.input_length - runtime.offset;
        count = remaining < length ? remaining : length;
        if (count > 3U)
                count = 3U;
        for (uint_32 index = 0; index < count; ++index)
                destination[index] = runtime.input[runtime.offset + index];
        runtime.offset += count;
        return (int_32) count;
}

static int_32 fake_close(int_32 fd)
{
        runtime.close_calls++;
        return fd == runtime.open_result ? runtime.close_result : -99;
}

static int_32 fake_exec(const char *path, const char *const argv[])
{
        runtime.exec_calls++;
        if (!equal(path, FROG_SYSTEM_INIT_GRAPHICAL_PATH) ||
            !argv || !equal(argv[0], FROG_SYSTEM_INIT_GRAPHICAL_PATH) ||
            argv[1])
                return -99;
        return runtime.exec_result;
}

static void capture(char byte, void *context)
{
        struct fake_runtime *state = context;

        if (state->output_length + 1U < sizeof(state->output))
                state->output[state->output_length++] = byte;
        state->output[state->output_length] = '\0';
}

static void reset(const char *input, uint_32 input_length)
{
        runtime = (struct fake_runtime) {
            .input = input,
            .input_length = input_length,
            .open_result = 4,
        };
}

static int parser_cases(void)
{
        static const char valid[] = "mode=graphical\n";
        static const char tty[] = "mode=tty\n";
        static const char missing_newline[] = "mode=graphical";
        static const char whitespace[] = "mode = graphical\n";
        static const char unknown[] = "display=graphical\n";
        static const char duplicate[] = "mode=graphical\nmode=graphical\n";
        static const char trailing[] = "mode=graphical\n#x";
        static const char nul[] = {'m', 'o', 'd', 'e', '=', 'g', '\0', 'x', '\n'};

        if (frog_system_init_parse_config(valid, sizeof(valid) - 1U) !=
                FROG_SYSTEM_INIT_OK ||
            frog_system_init_parse_config("", 0) != FROG_SYSTEM_INIT_MALFORMED ||
            frog_system_init_parse_config(missing_newline,
                                          sizeof(missing_newline) - 1U) !=
                FROG_SYSTEM_INIT_MALFORMED ||
            frog_system_init_parse_config(whitespace, sizeof(whitespace) - 1U) !=
                FROG_SYSTEM_INIT_MALFORMED ||
            frog_system_init_parse_config(unknown, sizeof(unknown) - 1U) !=
                FROG_SYSTEM_INIT_MALFORMED ||
            frog_system_init_parse_config(duplicate, sizeof(duplicate) - 1U) !=
                FROG_SYSTEM_INIT_MALFORMED ||
            frog_system_init_parse_config(trailing, sizeof(trailing) - 1U) !=
                FROG_SYSTEM_INIT_MALFORMED ||
            frog_system_init_parse_config(nul, sizeof(nul)) !=
                FROG_SYSTEM_INIT_MALFORMED ||
            frog_system_init_parse_config(tty, sizeof(tty) - 1U) !=
                FROG_SYSTEM_INIT_UNSUPPORTED_MODE)
                return 1;
        return 0;
}

static int runtime_cases(void)
{
        static const struct frog_system_init_ops ops = {
            .open = fake_open,
            .read = fake_read,
            .close = fake_close,
            .exec = fake_exec,
        };
        static const char valid[] = "mode=graphical\n";
        static const char overlong[] =
            "mode=graphical\nmode=graphical\nmode=graphical\n"
            "mode=graphical\nmode=graphical\n";

        reset(valid, sizeof(valid) - 1U);
        if (frog_system_init_run(&ops) != FROG_SYSTEM_INIT_EXEC_RETURNED ||
            runtime.close_calls != 1 || runtime.exec_calls != 1)
                return 2;
        reset(valid, sizeof(valid) - 1U);
        runtime.open_result = -2;
        if (frog_system_init_run(&ops) != FROG_SYSTEM_INIT_OPEN_FAILED ||
            runtime.close_calls || runtime.exec_calls)
                return 3;
        reset(valid, sizeof(valid) - 1U);
        runtime.read_failure = -5;
        if (frog_system_init_run(&ops) != FROG_SYSTEM_INIT_READ_FAILED ||
            runtime.close_calls != 1 || runtime.exec_calls)
                return 4;
        reset(valid, sizeof(valid) - 1U);
        runtime.close_result = -5;
        if (frog_system_init_run(&ops) != FROG_SYSTEM_INIT_CLOSE_FAILED ||
            runtime.exec_calls)
                return 5;
        reset(valid, sizeof(valid) - 1U);
        runtime.exec_result = -5;
        if (frog_system_init_run(&ops) != FROG_SYSTEM_INIT_EXEC_FAILED ||
            runtime.exec_calls != 1)
                return 6;
        reset(overlong, sizeof(overlong) - 1U);
        if (frog_system_init_run(&ops) != FROG_SYSTEM_INIT_MALFORMED ||
            runtime.offset != runtime.input_length || runtime.close_calls != 1 ||
            runtime.exec_calls)
                return 7;
        frog_system_init_report_status(FROG_SYSTEM_INIT_EXEC_FAILED, capture,
                                       &runtime);
        if (!equal(runtime.output, "system-init status=95\n"))
                return 8;
        return 0;
}

int main(void)
{
        int result = parser_cases();

        if (result)
                return result;
        return runtime_cases();
}
