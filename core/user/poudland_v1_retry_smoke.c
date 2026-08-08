#include <frog/errno.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

#define SERVER_WAIT_MS 60

static bool receive_request(int_32 server, frog_pkg_peer_id *peer_id,
                            uint_32 *request_id)
{
        struct frog_pkg_message package;
        struct poudland_v1_header *header;
        struct pollfd descriptor = {
            .fd = server,
            .events = POLLIN,
        };

        if (wait2(&descriptor, 1, SERVER_WAIT_MS) != 1 ||
            frog_pkg_server_receive(server, &package) <=
                (int_32) FROG_PKG_HEADER_SIZE)
                return false;
        header = (struct poudland_v1_header *) package.payload;
        *peer_id = package.peer_id;
        *request_id = header->request_id;
        return true;
}

static bool send_message(int_32 server, frog_pkg_peer_id peer_id,
                         const struct poudland_v1_message *message)
{
        uint_32 size = POUDLAND_V1_HEADER_SIZE +
                       message->header.payload_size;

        return frog_pkg_server_send(server, peer_id, message, size) > 0;
}

static void run_server(int_32 server)
{
        struct poudland_v1_message wire = { 0 };
        struct poudland_v1_error *error =
            (struct poudland_v1_error *) wire.payload;
        frog_pkg_peer_id peer_id = 0;
        uint_32 id_error = 0;
        uint_32 id_retry = 0;
        bool passed = receive_request(server, &peer_id, &id_error);

        wire.header.magic = POUDLAND_V1_MAGIC;
        wire.header.version = POUDLAND_V1_VERSION;
        wire.header.header_size = POUDLAND_V1_HEADER_SIZE;
        wire.header.type = POUDLAND_V1_MSG_ERROR;
        wire.header.request_id = id_error;
        wire.header.payload_size = sizeof(*error);
        error->status = -EPERM;
        error->failed_type = POUDLAND_V1_MSG_WINDOW_NEW;
        passed = send_message(server, peer_id, &wire) && passed;
        passed = receive_request(server, &peer_id, &id_retry) && passed;
        passed = wait2(NULL, 0, 5) == 0 && passed;
        wire.header.type = POUDLAND_V1_MSG_WINDOW_INIT;
        wire.header.request_id = id_retry;
        wire.header.payload_size = sizeof(struct poudland_v1_window_init);
        passed = send_message(server, peer_id, &wire) && passed;
        exit(passed ? 31 : 32);
}

static uint_32 pending_count(const struct poudland_v1_context *context)
{
        uint_32 count = 0;

        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++)
                count += context->pending[index].occupied ? 1U : 0U;
        return count;
}

static void run_client(int_32 server, int_32 child)
{
        struct poudland_v1_context context;
        struct poudland_v1_window_new request = { 0 };
        struct poudland_v1_message message;
        uint_32 id_error;
        uint_32 id_retry;
        int_32 status;
        bool passed;

        (void) close(server);
        poudland_v1_context_init(&context);
        context.fd = frog_pkg_connect("pl-retry", true);
        context.connected = context.fd >= 0;
        context.version = POUDLAND_V1_VERSION;
        passed = child > 0 &&
                 poudland_v1_begin_request(
                     &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                     sizeof(request), &id_error) == 0 &&
                 poudland_v1_wait_response(&context, id_error,
                                           SERVER_WAIT_MS, &message) ==
                     -EPERM &&
                 pending_count(&context) == 0 && context.connected &&
                 poudland_v1_begin_request(
                     &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                     sizeof(request), &id_retry) == 0 &&
                 poudland_v1_wait_response(&context, id_retry, 1,
                                           &message) == -ETIMEDOUT &&
                 pending_count(&context) == 1 && context.connected &&
                 poudland_v1_wait_response(&context, id_retry,
                                           SERVER_WAIT_MS, &message) == 0 &&
                 message.header.request_id == id_retry &&
                 pending_count(&context) == 0 &&
                 wait(&status) == child && status == 31;
        __asm__ volatile("int $0x93"
                         : "=a"(status)
                         : "a"(SYS_TEST_REPORT),
                           "b"(FROG_TEST_POUDLAND_V1_ERROR_TIMEOUT_REWAIT),
                           "c"(passed)
                         : "memory");
        if (context.fd >= 0)
                (void) close(context.fd);
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}

void _start(void)
{
        int_32 server = frog_pkg_bind("pl-retry", true);
        int_32 child = fork();

        if (child == 0)
                run_server(server);
        run_client(server, child);
}
