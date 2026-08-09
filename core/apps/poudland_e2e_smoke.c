#include <frog/errno.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/types.h>
#include <gua/poudland_v1.h>

#define REQUEST_TIMEOUT_MS 2000
#define HOLD_INTERVAL_MS   1000
#define OFFSCREEN_ORIGIN    (-4096)
#define DESKTOP_PROBE_ATTEMPTS 200U
#define DESKTOP_PROBE_WAIT_MS  10
#define DESKTOP_PROBE_TIMEOUT_MS 100
#define DESKTOP_WINDOW_FIRST_ID  66U
#define DESKTOP_WINDOW_SECOND_ID 67U
#define DESKTOP_WINDOW_CLOSED_ID 68U
#define UNKNOWN_REQUEST_ID 0xe2e00001U
#define UNKNOWN_REQUEST_TYPE 0x0000ffffU

static struct poudland_v1_context context_a;
static struct poudland_v1_context context_b;
static struct poudland_v1_context context_c;
static struct poudland_v1_context context_d;
static struct poudland_v1_context context_e;
static struct frog_pkg_message unknown_response;
static uint_32 active_window_ids[POUDLAND_V1_SERVER_WINDOW_MAX];
static uint_32 active_window_count;

static const struct poudland_v1_window_new desktop_windows[3] = {
    {
        .x = 100, .y = 100, .width = 200, .height = 160,
        .xrgb8888 = 0x00cc5533U,
    },
    {
        .x = 220, .y = 200, .width = 320, .height = 240,
        .xrgb8888 = 0x00339966U,
    },
    {
        .x = 420, .y = 200, .width = 200, .height = 800,
        .xrgb8888 = 0x0000ffffU,
    },
};

static int_32 raw_syscall1(uint_32 number, uint_32 argument)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(argument)
                         : "memory");
        return result;
}

static int_32 raw_syscall2(uint_32 number, uint_32 first, uint_32 second)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(first), "c"(second)
                         : "memory");
        return result;
}

static void finish(void) __attribute__((noreturn));

static void finish(void)
{
        testsyscall(0);
        for (;;)
                __asm__ volatile("pause");
}

static void require_case(uint_32 id, bool passed)
{
        (void) raw_syscall2(SYS_TEST_REPORT, id, passed);
        if (!passed)
                finish();
}

static pid_t launch(const char *path)
{
        pid_t child = fork();

        if (child == 0) {
                const char *argv[] = {path, NULL};

                (void) execv(path, argv);
                exit(127);
                for (;;)
                        __asm__ volatile("pause");
        }
        return child;
}

static bool window_init_matches(
    const struct poudland_v1_window_new *request,
    const struct poudland_v1_window_init *initialized)
{
        return initialized->window_id != 0 &&
               initialized->x == request->x &&
               initialized->y == request->y &&
               initialized->width == request->width &&
               initialized->height == request->height &&
               initialized->xrgb8888 == request->xrgb8888;
}

static bool create_desktop_windows(uint_32 window_ids[3])
{
        for (uint_32 index = 0; index < 3; index++) {
                struct poudland_v1_window_init initialized;

                if (poudland_v1_window_create(
                        &context_a, &desktop_windows[index],
                        REQUEST_TIMEOUT_MS, &initialized) != 0 ||
                    !window_init_matches(&desktop_windows[index],
                                         &initialized))
                        return false;
                for (uint_32 previous = 0; previous < index; previous++) {
                        if (window_ids[previous] == initialized.window_id)
                                return false;
                }
                window_ids[index] = initialized.window_id;
        }
        return true;
}

static bool no_pending(const struct poudland_v1_context *context)
{
        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++) {
                if (context->pending[index].occupied)
                        return false;
        }
        return true;
}

static bool remember_active_window(uint_32 window_id)
{
        if (window_id == 0 ||
            active_window_count >= POUDLAND_V1_SERVER_WINDOW_MAX)
                return false;
        for (uint_32 index = 0; index < active_window_count; index++) {
                if (active_window_ids[index] == window_id)
                        return false;
        }
        active_window_ids[active_window_count++] = window_id;
        return true;
}

static void prepare_offscreen_window(struct poudland_v1_window_new *request)
{
        request->x = OFFSCREEN_ORIGIN - (int_32) active_window_count;
        request->y = OFFSCREEN_ORIGIN;
        request->width = 1;
        request->height = 1;
        request->xrgb8888 = 0x00010101U + active_window_count;
}

static bool create_offscreen_window(struct poudland_v1_context *context)
{
        struct poudland_v1_window_new request;
        struct poudland_v1_window_init initialized;

        prepare_offscreen_window(&request);
        return poudland_v1_window_create(
                   context, &request, REQUEST_TIMEOUT_MS, &initialized) == 0 &&
               window_init_matches(&request, &initialized) &&
               remember_active_window(initialized.window_id);
}

static bool capacity_rejected(struct poudland_v1_context *context)
{
        struct poudland_v1_window_new request;
        struct poudland_v1_window_init initialized;

        prepare_offscreen_window(&request);
        return poudland_v1_window_create(
                   context, &request, REQUEST_TIMEOUT_MS, &initialized) ==
                   -ENOSPC &&
               context->connected && no_pending(context);
}

static bool invalid_geometry_rejected(void)
{
        struct poudland_v1_window_new request = desktop_windows[0];
        struct poudland_v1_message response;
        const struct poudland_v1_error *error;
        uint_32 request_id = 0;
        int_32 status;

        request.width = 0;
        status = poudland_v1_begin_request(
            &context_a, POUDLAND_V1_MSG_WINDOW_NEW, &request,
            sizeof(request), &request_id);
        if (status != 0)
                return false;
        status = poudland_v1_wait_response(
            &context_a, request_id, REQUEST_TIMEOUT_MS, &response);
        error = (const struct poudland_v1_error *) response.payload;
        return status == -EINVAL && context_a.connected &&
               response.header.type == POUDLAND_V1_MSG_ERROR &&
               response.header.request_id == request_id &&
               response.header.payload_size == sizeof(*error) &&
               error->status == -EINVAL &&
               error->failed_type == POUDLAND_V1_MSG_WINDOW_NEW &&
               no_pending(&context_a);
}

static bool unknown_type_rejected(void)
{
        struct poudland_v1_header request = {
            .magic = POUDLAND_V1_MAGIC,
            .version = POUDLAND_V1_VERSION,
            .header_size = POUDLAND_V1_HEADER_SIZE,
            .type = UNKNOWN_REQUEST_TYPE,
            .request_id = UNKNOWN_REQUEST_ID,
            .payload_size = 0,
        };
        struct pollfd descriptor = {
            .fd = context_a.fd,
            .events = POLLIN,
            .revents = 0,
        };
        const struct poudland_v1_header *header;
        const struct poudland_v1_error *error;
        int_32 status;

        if (!no_pending(&context_a) ||
            frog_pkg_client_send(context_a.fd, &request, sizeof(request)) !=
                (int_32) (FROG_PKG_HEADER_SIZE + sizeof(request)))
                return false;
        status = wait2(&descriptor, 1, REQUEST_TIMEOUT_MS);
        if (status != 1 || !(descriptor.revents & POLLIN) ||
            (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)))
                return false;
        status = read(context_a.fd, &unknown_response,
                      sizeof(unknown_response));
        if (status != (int_32) (FROG_PKG_HEADER_SIZE +
                                POUDLAND_V1_HEADER_SIZE +
                                sizeof(struct poudland_v1_error)) ||
            unknown_response.peer_id != 0 ||
            unknown_response.event != FROG_PKG_DATA ||
            unknown_response.payload_size !=
                POUDLAND_V1_HEADER_SIZE +
                    sizeof(struct poudland_v1_error))
                return false;
        header = (const struct poudland_v1_header *)
            unknown_response.payload;
        error = (const struct poudland_v1_error *)
            (unknown_response.payload + POUDLAND_V1_HEADER_SIZE);
        return header->magic == POUDLAND_V1_MAGIC &&
               header->version == POUDLAND_V1_VERSION &&
               header->header_size == POUDLAND_V1_HEADER_SIZE &&
               header->type == POUDLAND_V1_MSG_ERROR &&
               header->request_id == UNKNOWN_REQUEST_ID &&
               header->payload_size == sizeof(*error) &&
               error->status == -EPROTO &&
               error->failed_type == UNKNOWN_REQUEST_TYPE &&
               context_a.connected && no_pending(&context_a);
}

static bool session_limit_enforced(const uint_32 window_ids[3])
{
        if (active_window_count != 0 ||
            !remember_active_window(window_ids[0]) ||
            !remember_active_window(window_ids[1]))
                return false;
        for (uint_32 index = 2;
             index < POUDLAND_V1_SESSION_WINDOW_MAX; index++) {
                if (!create_offscreen_window(&context_a))
                        return false;
        }
        return active_window_count == POUDLAND_V1_SESSION_WINDOW_MAX &&
               capacity_rejected(&context_a);
}

static bool fill_session(struct poudland_v1_context *context)
{
        for (uint_32 index = 0;
             index < POUDLAND_V1_SESSION_WINDOW_MAX; index++) {
                if (!create_offscreen_window(context))
                        return false;
        }
        return true;
}

static bool connect_context(struct poudland_v1_context *context)
{
        poudland_v1_context_init(context);
        return poudland_v1_connect(
                   context, "compositor", REQUEST_TIMEOUT_MS, 0) == 0;
}

static bool server_limit_enforced(void)
{
        if (!fill_session(&context_b) ||
            !connect_context(&context_c) || !fill_session(&context_c) ||
            !connect_context(&context_d) || !fill_session(&context_d) ||
            active_window_count != POUDLAND_V1_SERVER_WINDOW_MAX ||
            !connect_context(&context_e))
                return false;
        return capacity_rejected(&context_e);
}

static bool context_is_disconnected(
    const struct poudland_v1_context *context)
{
        return context->fd == -1 && !context->connected &&
               no_pending(context);
}

static bool disconnect_cleanup_enforced(uint_32 old_window_id)
{
        return poudland_v1_disconnect(&context_a) == 0 &&
               context_is_disconnected(&context_a) &&
               poudland_v1_window_close(
                   &context_e, old_window_id, REQUEST_TIMEOUT_MS) ==
                   -ENOENT &&
               context_e.connected && no_pending(&context_e);
}

static bool disconnect_remaining_clients(void)
{
        return poudland_v1_disconnect(&context_b) == 0 &&
               poudland_v1_disconnect(&context_c) == 0 &&
               poudland_v1_disconnect(&context_d) == 0 &&
               poudland_v1_disconnect(&context_e) == 0 &&
               context_is_disconnected(&context_b) &&
               context_is_disconnected(&context_c) &&
               context_is_disconnected(&context_d) &&
               context_is_disconnected(&context_e);
}

static bool probe_desktop_window(uint_32 window_id, int_32 *status)
{
        *status = poudland_v1_window_close(
            &context_a, window_id, DESKTOP_PROBE_TIMEOUT_MS);
        return context_a.connected && no_pending(&context_a);
}

static bool desktop_two_live(void)
{
        int_32 first_status;
        int_32 second_status;
        int_32 closed_status;

        if (!connect_context(&context_a))
                return false;
        for (uint_32 attempt = 0;
             attempt < DESKTOP_PROBE_ATTEMPTS; attempt++) {
                if (!probe_desktop_window(
                        DESKTOP_WINDOW_FIRST_ID, &first_status) ||
                    (first_status != -ENOENT && first_status != -EPERM))
                        return false;
                if (!probe_desktop_window(
                        DESKTOP_WINDOW_SECOND_ID, &second_status) ||
                    (second_status != -ENOENT && second_status != -EPERM))
                        return false;
                if (!probe_desktop_window(
                        DESKTOP_WINDOW_CLOSED_ID, &closed_status) ||
                    (closed_status != -ENOENT && closed_status != -EPERM))
                        return false;
                if (first_status == -EPERM && second_status == -EPERM &&
                    closed_status == -ENOENT)
                        return poudland_v1_disconnect(&context_a) == 0 &&
                               context_is_disconnected(&context_a);
                if (wait2(NULL, 0, DESKTOP_PROBE_WAIT_MS) != 0)
                        return false;
        }
        return false;
}

int main(int argc, char **argv)
{
        static const char compositor_path[] = "/test/compositor";
        static const char desktop_path[] = "/test/desktop";
        uint_32 window_ids[3] = {0};
        pid_t compositor;
        pid_t desktop;

        (void) argc;
        (void) argv;
        require_case(FROG_TEST_POUDLAND_E2E_HARNESS_EXEC, true);

        compositor = launch(compositor_path);
        require_case(FROG_TEST_POUDLAND_E2E_COMPOSITOR_FORK,
                     compositor > 0);

        poudland_v1_context_init(&context_a);
        require_case(
            FROG_TEST_POUDLAND_E2E_CLIENT_A_CONNECT,
            poudland_v1_connect(&context_a, "compositor",
                                REQUEST_TIMEOUT_MS, 0) == 0);
        require_case(FROG_TEST_POUDLAND_E2E_THREE_WINDOWS,
                     create_desktop_windows(window_ids));
        require_case(
            FROG_TEST_POUDLAND_E2E_CLOSE_THIRD,
            poudland_v1_window_close(&context_a, window_ids[2],
                                     REQUEST_TIMEOUT_MS) == 0);
        require_case(
            FROG_TEST_POUDLAND_E2E_CLOSE_STALE,
            poudland_v1_window_close(&context_a, window_ids[2],
                                     REQUEST_TIMEOUT_MS) == -ENOENT &&
                context_a.connected);

        poudland_v1_context_init(&context_b);
        require_case(
            FROG_TEST_POUDLAND_E2E_CLIENT_B_CONNECT,
            poudland_v1_connect(&context_b, "compositor",
                                REQUEST_TIMEOUT_MS, 0) == 0);
        require_case(
            FROG_TEST_POUDLAND_E2E_FOREIGN_CLOSE,
            poudland_v1_window_close(&context_b, window_ids[0],
                                     REQUEST_TIMEOUT_MS) == -EPERM &&
                context_b.connected);
        require_case(FROG_TEST_POUDLAND_E2E_INVALID_GEOMETRY,
                     invalid_geometry_rejected());
        require_case(FROG_TEST_POUDLAND_E2E_UNKNOWN_TYPE,
                     unknown_type_rejected());
        require_case(FROG_TEST_POUDLAND_E2E_SESSION_LIMIT,
                     session_limit_enforced(window_ids));
        require_case(FROG_TEST_POUDLAND_E2E_SERVER_LIMIT,
                     server_limit_enforced());
        require_case(FROG_TEST_POUDLAND_E2E_DISCONNECT_CLEANUP,
                     disconnect_cleanup_enforced(window_ids[0]));
        require_case(
            FROG_TEST_POUDLAND_E2E_DISCONNECT,
            disconnect_remaining_clients());

        desktop = launch(desktop_path);
        require_case(FROG_TEST_POUDLAND_E2E_DESKTOP_FORK, desktop > 0);
        require_case(FROG_TEST_POUDLAND_E2E_DESKTOP_TWO_LIVE,
                     desktop_two_live());
        require_case(FROG_TEST_POUDLAND_E2E_HOLD,
                     wait2(NULL, 0, HOLD_INTERVAL_MS) == 0);
        require_case(
            FROG_TEST_POUDLAND_E2E_DESKTOP_SYNC,
            raw_syscall1(SYS_TEST_SYNC,
                         FROG_TEST_POUDLAND_E2E_DESKTOP_LAUNCHED) == 0);
        for (;;) {
                if (wait2(NULL, 0, HOLD_INTERVAL_MS) != 0) {
                        require_case(FROG_TEST_POUDLAND_E2E_HOLD, false);
                }
        }
}
