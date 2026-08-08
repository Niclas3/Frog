#include <frog/errno.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

#define SERVER_WAIT_MS 60

#if defined(POUDLAND_V1_TEST_CREATE)
#define TEST_SERVICE "pl-create"
#define TEST_ID      FROG_TEST_POUDLAND_V1_WINDOW_LIFECYCLE
#define REQUEST_TYPE POUDLAND_V1_MSG_WINDOW_NEW
#define REQUEST_SIZE sizeof(struct poudland_v1_window_new)
#elif defined(POUDLAND_V1_TEST_CLOSE)
#define TEST_SERVICE "pl-close"
#define TEST_ID      FROG_TEST_POUDLAND_V1_WINDOW_CLOSE
#define REQUEST_TYPE POUDLAND_V1_MSG_WINDOW_CLOSE
#define REQUEST_SIZE sizeof(struct poudland_v1_window_close)
#elif defined(POUDLAND_V1_TEST_ERROR)
#define TEST_SERVICE "pl-error"
#define TEST_ID      FROG_TEST_POUDLAND_V1_ERROR_RESPONSE
#define REQUEST_TYPE POUDLAND_V1_MSG_WINDOW_NEW
#define REQUEST_SIZE sizeof(struct poudland_v1_window_new)
#else
#error one Poudland wrapper test mode is required
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

static void run_server(int_32 server)
{
        struct frog_pkg_message package;
        struct poudland_v1_header *request;
        struct poudland_v1_message response;
        struct pollfd descriptor = {
            .fd = server,
            .events = POLLIN,
        };
        int_32 status = wait2(&descriptor, 1, SERVER_WAIT_MS);

        if (status != 1 || !(descriptor.revents & POLLIN) ||
            frog_pkg_server_receive(server, &package) <= 0)
                exit(32);
        request = (struct poudland_v1_header *) package.payload;
        response.header.magic = POUDLAND_V1_MAGIC;
        response.header.version = POUDLAND_V1_VERSION;
        response.header.header_size = POUDLAND_V1_HEADER_SIZE;
        response.header.request_id = request->request_id;
#if defined(POUDLAND_V1_TEST_CREATE)
        response.header.type = POUDLAND_V1_MSG_WINDOW_INIT;
        response.header.payload_size = sizeof(struct poudland_v1_window_init);
        *(struct poudland_v1_window_init *) response.payload =
            (struct poudland_v1_window_init) {
                .window_id = 9,
                .x = 4,
                .y = 5,
                .width = 64,
                .height = 48,
                .xrgb8888 = 0x00112233U,
            };
#elif defined(POUDLAND_V1_TEST_CLOSE)
        response.header.type = POUDLAND_V1_MSG_WINDOW_CLOSED;
        response.header.payload_size =
            sizeof(struct poudland_v1_window_closed);
        ((struct poudland_v1_window_closed *) response.payload)->window_id =
            9;
        if (((struct poudland_v1_window_close *)
                 (package.payload + POUDLAND_V1_HEADER_SIZE))->window_id != 9)
                exit(32);
#else
        response.header.type = POUDLAND_V1_MSG_ERROR;
        response.header.payload_size = sizeof(struct poudland_v1_error);
        ((struct poudland_v1_error *) response.payload)->status = -EPERM;
        ((struct poudland_v1_error *) response.payload)->failed_type =
            POUDLAND_V1_MSG_WINDOW_NEW;
#endif
        status = frog_pkg_server_send(
            server, package.peer_id, &response,
            POUDLAND_V1_HEADER_SIZE + response.header.payload_size);
        exit(status > 0 ? 31 : 32);
}

#if defined(POUDLAND_V1_TEST_ERROR)
static uint_32 pending_count(const struct poudland_v1_context *context)
{
        uint_32 count = 0;

        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++)
                count += context->pending[index].occupied ? 1U : 0U;
        return count;
}
#endif

static void run_client(int_32 server, int_32 child)
{
        struct poudland_v1_context context;
#if !defined(POUDLAND_V1_TEST_CLOSE)
        struct poudland_v1_window_new request = {
            .x = 4,
            .y = 5,
            .width = 64,
            .height = 48,
            .xrgb8888 = 0x00112233U,
        };
        struct poudland_v1_window_init result;
#endif
        int_32 child_status;
        bool passed;

        (void) close(server);
        poudland_v1_context_init(&context);
        context.fd = frog_pkg_connect(TEST_SERVICE, true);
        context.connected = context.fd >= 0;
        context.version = POUDLAND_V1_VERSION;
#if defined(POUDLAND_V1_TEST_CREATE)
        passed = child > 0 && context.connected &&
                 poudland_v1_window_create(&context, &request, SERVER_WAIT_MS,
                                           &result) == 0 &&
                 result.window_id == 9 && result.x == 4 && result.y == 5 &&
                 result.width == 64 && result.height == 48 &&
                 result.xrgb8888 == 0x00112233U && context.connected &&
                 context.fd >= 0;
#elif defined(POUDLAND_V1_TEST_CLOSE)
        passed = child > 0 && context.connected &&
                 poudland_v1_window_close(&context, 9, SERVER_WAIT_MS) == 0;
#else
        passed = child > 0 && context.connected &&
                 poudland_v1_window_create(&context, &request, SERVER_WAIT_MS,
                                           &result) == -EPERM &&
                 context.connected && context.fd >= 0 &&
                 pending_count(&context) == 0;
#endif
        passed = passed && close(context.fd) == 0 &&
                 wait(&child_status) == child && child_status == 31;
        report(passed);
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
