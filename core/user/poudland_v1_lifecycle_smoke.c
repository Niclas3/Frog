#include <frog/errno.h>
#include <frog/packagefs.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

static void report(uint_32 id, bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT), "b"(id), "c"(passed)
                         : "memory");
}

static uint_32 pending_count(const struct poudland_v1_context *context)
{
        uint_32 count = 0;

        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++) {
                if (context->pending[index].occupied)
                        count++;
        }
        return count;
}

void _start(void)
{
        struct poudland_v1_context context;
        struct poudland_v1_window_new request = {
            .x = 0,
            .y = 0,
            .width = 64,
            .height = 64,
            .xrgb8888 = 0x00112233U,
        };
        struct poudland_v1_window_init init;
        int_32 server = frog_pkg_bind("pl-timeout", true);
        bool passed;

        poudland_v1_context_init(&context);
        context.fd = frog_pkg_connect("pl-timeout", true);
        context.connected = context.fd >= 0;
        context.version = POUDLAND_V1_VERSION;
        passed = server >= 0 && context.connected &&
                 poudland_v1_window_create(&context, &request, 1, &init) ==
                     -ETIMEDOUT &&
                 context.fd == -1 && !context.connected &&
                 pending_count(&context) == 0;
        report(FROG_TEST_POUDLAND_V1_WRAPPER_TIMEOUT_RESET, passed);
        if (context.fd >= 0)
                (void) close(context.fd);
        if (server >= 0)
                (void) close(server);
        (void) testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}
