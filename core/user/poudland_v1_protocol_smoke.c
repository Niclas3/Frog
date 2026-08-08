#include <frog/errno.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

#define SERVER_WAIT_MS 60

#if defined(POUDLAND_V1_TEST_PROTOCOL)
#define TEST_ID      FROG_TEST_POUDLAND_V1_MALFORMED_FRAME
#define TEST_SERVICE "pl-proto"
#elif defined(POUDLAND_V1_TEST_FATAL)
#define TEST_ID      FROG_TEST_POUDLAND_V1_ROUTING_FATAL
#define TEST_SERVICE "pl-fatal"
#elif defined(POUDLAND_V1_TEST_OVERFLOW)
#define TEST_ID      FROG_TEST_POUDLAND_V1_INBOX_OVERFLOW
#define TEST_SERVICE "pl-overflow"
#elif defined(POUDLAND_V1_TEST_HUP)
#define TEST_ID      FROG_TEST_POUDLAND_V1_HUP_DRAIN
#define TEST_SERVICE "pl-hup"
#else
#error one Poudland protocol test mode is required
#endif

static void report(bool passed)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_REPORT), "b"(TEST_ID),
                           "c"(passed)
                         : "memory");
}

static uint_32 pending_count(const struct poudland_v1_context *context)
{
        uint_32 count = 0;

        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++)
                count += context->pending[index].occupied ? 1U : 0U;
        return count;
}

static bool attach(struct poudland_v1_context *context)
{
        poudland_v1_context_init(context);
        context->fd = frog_pkg_connect(TEST_SERVICE, true);
        context->connected = context->fd >= 0;
        context->version = POUDLAND_V1_VERSION;
        return context->connected;
}

static bool receive_data(int_32 server, frog_pkg_peer_id *peer_id)
{
        struct frog_pkg_message package;
#if defined(POUDLAND_V1_TEST_OVERFLOW) || defined(POUDLAND_V1_TEST_HUP)
        struct pollfd descriptor = {
            .fd = server,
            .events = POLLIN,
        };
#endif

        for (uint_32 attempt = 0; attempt < 3; attempt++) {
#if defined(POUDLAND_V1_TEST_OVERFLOW) || defined(POUDLAND_V1_TEST_HUP)
                if (wait2(&descriptor, 1, SERVER_WAIT_MS) != 1)
                        return false;
#endif
                int_32 status = frog_pkg_server_receive(server, &package);

                if (status > (int_32) FROG_PKG_HEADER_SIZE &&
                    package.payload_size != 0) {
                        *peer_id = package.peer_id;
                        return true;
                }
                if (status < 0)
                        return false;
        }
        return false;
}

static bool send_wire(int_32 server, frog_pkg_peer_id peer_id,
                      const struct poudland_v1_message *wire,
                      uint_32 wire_size)
{
        return frog_pkg_server_send(server, peer_id, wire, wire_size) ==
               (int_32) (FROG_PKG_HEADER_SIZE + wire_size);
}

#if defined(POUDLAND_V1_TEST_PROTOCOL) || defined(POUDLAND_V1_TEST_FATAL)
static bool invalid_case(int_32 server, uint_32 kind)
{
        struct poudland_v1_context context;
        struct poudland_v1_window_new request = {
            .width = 1,
            .height = 1,
        };
        struct poudland_v1_message response;
        struct poudland_v1_message wire = { 0 };
        frog_pkg_peer_id peer_id = 0;
        uint_32 request_id = 0;
        uint_32 wire_size = POUDLAND_V1_HEADER_SIZE +
                            sizeof(struct poudland_v1_window_init);
        bool passed = attach(&context) &&
                      poudland_v1_begin_request(
                          &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                          sizeof(request), &request_id) == 0 &&
                      receive_data(server, &peer_id);

        wire.header.magic = POUDLAND_V1_MAGIC;
        wire.header.version = POUDLAND_V1_VERSION;
        wire.header.header_size = POUDLAND_V1_HEADER_SIZE;
        wire.header.type = POUDLAND_V1_MSG_WINDOW_INIT;
        wire.header.request_id = request_id;
        wire.header.payload_size = sizeof(struct poudland_v1_window_init);
#if defined(POUDLAND_V1_TEST_PROTOCOL)
        if (kind == 0)
                wire.header.magic = 0;
        else if (kind == 1)
                wire.header.version = 2;
        else if (kind == 2)
                wire.header.header_size = POUDLAND_V1_HEADER_SIZE - 1U;
        else
                wire_size--;
#else
        if (kind == 0)
                wire.header.request_id++;
        else {
                wire.header.type = POUDLAND_V1_MSG_WINDOW_CLOSED;
                wire.header.payload_size =
                    sizeof(struct poudland_v1_window_closed);
                wire_size = POUDLAND_V1_HEADER_SIZE +
                            wire.header.payload_size;
        }
#endif
        passed = passed &&
                 send_wire(server, peer_id, &wire, wire_size) &&
                 poudland_v1_wait_response(&context, request_id, 20,
                                           &response) == -EPROTO &&
                 context.fd == -1 && !context.connected &&
                 pending_count(&context) == 0;
        (void) receive_data(server, &peer_id);
        return passed;
}
#endif

#if defined(POUDLAND_V1_TEST_OVERFLOW) || defined(POUDLAND_V1_TEST_HUP)
static void run_server(int_32 server)
{
        struct poudland_v1_message wire = { 0 };
        frog_pkg_peer_id peer_id = 0;
        bool passed;

        wire.header.magic = POUDLAND_V1_MAGIC;
        wire.header.version = POUDLAND_V1_VERSION;
        wire.header.header_size = POUDLAND_V1_HEADER_SIZE;
        wire.header.type = POUDLAND_V1_MSG_WINDOW_INIT;
        wire.header.payload_size = sizeof(struct poudland_v1_window_init);
#if defined(POUDLAND_V1_TEST_OVERFLOW)
        passed = true;
        for (uint_32 index = 0; index < 10; index++)
                passed = receive_data(server, &peer_id) && passed;
        for (uint_32 id = 1; id <= 9; id++) {
                wire.header.request_id = id;
                passed = send_wire(
                             server, peer_id, &wire,
                             POUDLAND_V1_HEADER_SIZE +
                                 wire.header.payload_size) &&
                         passed;
        }
#else
        passed = receive_data(server, &peer_id);
        wire.header.request_id = 1;
        passed = send_wire(server, peer_id, &wire,
                           POUDLAND_V1_HEADER_SIZE +
                               wire.header.payload_size) &&
                 passed;
#endif
        (void) close(server);
        exit(passed ? 31 : 32);
}

static void run_client(int_32 server, int_32 child)
{
        struct poudland_v1_context context;
        struct poudland_v1_window_new request = { 0 };
        struct poudland_v1_message response;
        uint_32 request_id = 0;
        int_32 child_status;
        bool passed;

        (void) close(server);
        passed = child > 0 && attach(&context);
#if defined(POUDLAND_V1_TEST_OVERFLOW)
        for (uint_32 index = 0; index < 10; index++)
                passed = poudland_v1_begin_request(
                             &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                             sizeof(request), &request_id) == 0 &&
                         request_id == index + 1U && passed;
        passed = poudland_v1_wait_response(&context, request_id,
                                           SERVER_WAIT_MS,
                                           &response) == -ENOBUFS &&
                 context.fd == -1 && !context.connected &&
                 pending_count(&context) == 0 && passed;
#else
        passed = passed &&
                 poudland_v1_begin_request(
                     &context, POUDLAND_V1_MSG_WINDOW_NEW, &request,
                     sizeof(request), &request_id) == 0 &&
                 poudland_v1_wait_response(&context, request_id,
                                           SERVER_WAIT_MS,
                                           &response) == 0 &&
                 response.header.request_id == request_id &&
                 context.connected && pending_count(&context) == 0 &&
                 poudland_v1_next_event(&context, 20, &response) ==
                     -ECONNRESET &&
                 context.fd == -1 && !context.connected;
#endif
        passed = wait(&child_status) == child && child_status == 31 && passed;
        report(passed);
        if (context.fd >= 0)
                (void) close(context.fd);
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}

void _start(void)
{
        int_32 server = frog_pkg_bind(TEST_SERVICE, true);
        int_32 child = fork();

        if (child == 0)
                run_server(server);
        run_client(server, child);
}
#else
void _start(void)
{
        int_32 server = frog_pkg_bind(TEST_SERVICE, true);
        bool passed = server >= 0;

#if defined(POUDLAND_V1_TEST_PROTOCOL)
        for (uint_32 kind = 0; kind < 4; kind++)
                passed = invalid_case(server, kind) && passed;
#else
        for (uint_32 kind = 0; kind < 2; kind++)
                passed = invalid_case(server, kind) && passed;
#endif
        report(passed);
        if (server >= 0)
                (void) close(server);
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}
#endif
