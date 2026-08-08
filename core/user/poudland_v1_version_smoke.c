#include <frog/errno.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

static void report(bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT),
                           "b"(FROG_TEST_POUDLAND_V1_INCOMPATIBLE_VERSION),
                           "c"(passed)
                         : "memory");
}

static void version_server(int_32 server)
{
        struct frog_pkg_message package;
        struct poudland_v1_header *request;
        struct poudland_v1_message response;
        struct poudland_v1_welcome *welcome =
            (struct poudland_v1_welcome *) response.payload;
        struct pollfd descriptor = { .fd = server, .events = POLLIN,
                                     .revents = 0 };
        bool passed = wait2(&descriptor, 1, 60) == 1 &&
                      (descriptor.revents & POLLIN) &&
                      frog_pkg_server_receive(server, &package) ==
                          FROG_PKG_HEADER_SIZE + POUDLAND_V1_HEADER_SIZE +
                              sizeof(struct poudland_v1_hello);

        request = (struct poudland_v1_header *) package.payload;
        response.header.magic = POUDLAND_V1_MAGIC;
        response.header.version = POUDLAND_V1_VERSION;
        response.header.header_size = POUDLAND_V1_HEADER_SIZE;
        response.header.type = POUDLAND_V1_MSG_WELCOME;
        response.header.request_id = request->request_id;
        response.header.payload_size = sizeof(*welcome);
        welcome->selected_version = 2;
        welcome->reserved = 0;
        welcome->display_width = 640;
        welcome->display_height = 480;
        welcome->capabilities = 0;
        passed = passed && request->type == POUDLAND_V1_MSG_HELLO &&
                 frog_pkg_server_send(
                     server, package.peer_id, &response,
                     POUDLAND_V1_HEADER_SIZE + sizeof(*welcome)) ==
                     FROG_PKG_HEADER_SIZE + POUDLAND_V1_HEADER_SIZE +
                         sizeof(*welcome);
        (void) close(server);
        exit(passed ? 37 : 38);
}

static void version_client(int_32 server, int_32 child)
{
        struct poudland_v1_context context;
        int_32 status;
        bool passed;

        (void) close(server);
        poudland_v1_context_init(&context);
        passed = poudland_v1_connect(&context, "pl-version", 60, 0) ==
                     -EPROTONOSUPPORT &&
                 context.fd == -1 && !context.connected &&
                 wait(&status) == child && status == 37;
        report(passed);
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}

void _start(void)
{
        int_32 server = frog_pkg_bind("pl-version", true);
        int_32 child = fork();

        if (child == 0)
                version_server(server);
        version_client(server, child);
}
