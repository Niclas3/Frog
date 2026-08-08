#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

#define CONNECT_TIMEOUT_MS 100
#define SERVER_WAIT_MS     80

static void report(uint_32 id, bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT), "b"(id), "c"(passed)
                         : "memory");
}

static void finish(void)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TESTSYSCALL), "b"(0)
                         : "memory");
        for (;;)
                __asm__ volatile("pause");
}

static bool server_receive(int_32 server, uint_32 expected_type,
                           frog_pkg_peer_id *peer_id, uint_32 *request_id,
                           void *payload, uint_32 payload_size)
{
        struct frog_pkg_message package;
        struct poudland_v1_header *header;
        struct pollfd descriptor = { .fd = server, .events = POLLIN,
                                     .revents = 0 };
        int_32 result = wait2(&descriptor, 1, SERVER_WAIT_MS);

        if (result != 1 || !(descriptor.revents & POLLIN))
                return false;
        result = frog_pkg_server_receive(server, &package);
        if (result != (int_32) (FROG_PKG_HEADER_SIZE +
                                POUDLAND_V1_HEADER_SIZE + payload_size) ||
            package.payload_size != POUDLAND_V1_HEADER_SIZE + payload_size)
                return false;
        header = (struct poudland_v1_header *) package.payload;
        if (header->magic != POUDLAND_V1_MAGIC ||
            header->version != POUDLAND_V1_VERSION ||
            header->header_size != POUDLAND_V1_HEADER_SIZE ||
            header->type != expected_type || header->request_id == 0 ||
            header->payload_size != payload_size)
                return false;
        *peer_id = package.peer_id;
        *request_id = header->request_id;
        for (uint_32 index = 0; index < payload_size; index++)
                ((uint_8 *) payload)[index] =
                    package.payload[POUDLAND_V1_HEADER_SIZE + index];
        return true;
}

static bool server_send(int_32 server, frog_pkg_peer_id peer_id,
                        uint_32 type, uint_32 request_id,
                        const void *payload, uint_32 payload_size)
{
        struct poudland_v1_message message;

        message.header.magic = POUDLAND_V1_MAGIC;
        message.header.version = POUDLAND_V1_VERSION;
        message.header.header_size = POUDLAND_V1_HEADER_SIZE;
        message.header.type = type;
        message.header.request_id = request_id;
        message.header.payload_size = payload_size;
        for (uint_32 index = 0; index < payload_size; index++)
                message.payload[index] = ((const uint_8 *) payload)[index];
        return frog_pkg_server_send(
                   server, peer_id, &message,
                   POUDLAND_V1_HEADER_SIZE + payload_size) ==
               (int_32) (FROG_PKG_HEADER_SIZE +
                         POUDLAND_V1_HEADER_SIZE + payload_size);
}

static void happy_server(void)
{
        struct poudland_v1_hello hello;
        struct poudland_v1_welcome welcome = {
            .selected_version = POUDLAND_V1_VERSION,
            .reserved = 0,
            .display_width = 640,
            .display_height = 480,
            .capabilities = POUDLAND_V1_CAP_CONFIGURE |
                            POUDLAND_V1_CAP_POINTER |
                            POUDLAND_V1_CAP_KEYBOARD,
        };
        struct frog_pkg_message package;
        frog_pkg_peer_id peer_id;
        uint_32 request_id;
        int_32 server;
        bool passed;

        (void) wait2(NULL, 0, 5);
        server = frog_pkg_bind("pl-connect", true);
        passed = server >= 0 &&
                 server_receive(server, POUDLAND_V1_MSG_HELLO, &peer_id,
                                &request_id, &hello, sizeof(hello)) &&
                 hello.min_version == POUDLAND_V1_VERSION &&
                 hello.max_version == POUDLAND_V1_VERSION &&
                 server_send(server, peer_id, POUDLAND_V1_MSG_WELCOME,
                             request_id, &welcome, sizeof(welcome));
        if (passed) {
                struct pollfd descriptor = { .fd = server, .events = POLLIN,
                                             .revents = 0 };

                passed = wait2(&descriptor, 1, SERVER_WAIT_MS) == 1 &&
                         (descriptor.revents & POLLIN) &&
                         frog_pkg_server_receive(server, &package) ==
                             FROG_PKG_HEADER_SIZE &&
                         package.event == FROG_PKG_DISCONNECT;
        }
        if (server >= 0)
                (void) close(server);
        exit(passed ? 31 : 32);
}

static void run_client(int_32 child)
{
        struct poudland_v1_context context;
        int_32 status;
        bool passed;

        poudland_v1_context_init(&context);
        passed = child > 0 &&
                 poudland_v1_connect(
                     &context, "pl-connect", CONNECT_TIMEOUT_MS,
                     POUDLAND_V1_CAP_CONFIGURE |
                         POUDLAND_V1_CAP_POINTER |
                         POUDLAND_V1_CAP_KEYBOARD) == 0 &&
                 context.fd >= 0 && context.connected &&
                 context.version == POUDLAND_V1_VERSION &&
                 context.display_width == 640 &&
                 context.display_height == 480 &&
                 poudland_v1_disconnect(&context) == 0 && context.fd == -1 &&
                 !context.connected && wait(&status) == child && status == 31;
        report(FROG_TEST_POUDLAND_V1_CONNECT_HANDSHAKE, passed);
        finish();
}

void _start(void)
{
        int_32 child = fork();

        if (child == 0)
                happy_server();
        run_client(child);
}
