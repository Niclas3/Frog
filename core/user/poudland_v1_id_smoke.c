#include <frog/packagefs.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

static void report(bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT),
                           "b"(FROG_TEST_POUDLAND_V1_ID_WRAP_SKIP),
                           "c"(passed)
                         : "memory");
}

void _start(void)
{
        struct poudland_v1_context context;
        struct poudland_v1_window_new request = {
            .width = 1,
            .height = 1,
        };
        uint_32 id_a;
        uint_32 id_b;
        uint_32 id_c;
        int_32 server = frog_pkg_bind("pl-id", true);
        bool passed;

        poudland_v1_context_init(&context);
        context.fd = frog_pkg_connect("pl-id", true);
        context.connected = context.fd >= 0;
        context.version = POUDLAND_V1_VERSION;
        passed = server >= 0 && context.connected &&
                 poudland_v1_begin_request(
                     &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                     sizeof(request), &id_a) == 0 &&
                 id_a == 1;
        context.next_request_id = 0xffffffffU;
        passed = passed &&
                 poudland_v1_begin_request(
                     &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                     sizeof(request), &id_b) == 0 &&
                 id_b == 0xffffffffU &&
                 poudland_v1_begin_request(
                     &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                     sizeof(request), &id_c) == 0 &&
                 id_c == 2;
        report(passed);
        (void) poudland_v1_disconnect(&context);
        (void) close(server);
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}
