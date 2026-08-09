#include <frog/errno.h>
#include <gua/poudland_v1.h>

struct pollfd;

#define DESKTOP_TIMEOUT_MS 2000

enum call_kind {
        CALL_INIT = 1,
        CALL_CONNECT,
        CALL_CREATE,
        CALL_CLOSE,
        CALL_WAIT,
        CALL_DISCONNECT,
};

struct expected_window {
        int_32 x;
        int_32 y;
        uint_32 width;
        uint_32 height;
        uint_32 color;
        uint_32 id;
};

static const struct expected_window expected_windows[3] = {
    {100, 100, 200, 160, 0x00cc5533U, 101},
    {220, 200, 320, 240, 0x00339966U, 202},
    {420, 200, 200, 800, 0x0000ffffU, 303},
};

static uint_32 calls[16];
static uint_32 call_count;
static uint_32 create_count;
static uint_32 wait_count;
static uint_32 disconnect_count;
static uint_32 mismatch;
static uint_32 fail_call;
static bool bad_echo;
static bool duplicate_id;
static bool zero_id;

int desktop_entry(int argc, char **argv);

static bool string_equal(const char *left, const char *right)
{
        while (*left && *right) {
                if (*left++ != *right++)
                        return false;
        }
        return *left == *right;
}

static void record(uint_32 call)
{
        if (call_count < sizeof(calls) / sizeof(calls[0]))
                calls[call_count++] = call;
        else
                mismatch = 1;
}

static void reset_test(void)
{
        uint_32 index;

        for (index = 0; index < sizeof(calls) / sizeof(calls[0]); ++index)
                calls[index] = 0;
        call_count = 0;
        create_count = 0;
        wait_count = 0;
        disconnect_count = 0;
        mismatch = 0;
        fail_call = 0;
        bad_echo = false;
        duplicate_id = false;
        zero_id = false;
}

void poudland_v1_context_init(struct poudland_v1_context *context)
{
        record(CALL_INIT);
        context->fd = -1;
        context->connected = 0;
}

int_32 poudland_v1_connect(struct poudland_v1_context *context,
                          const char *service, int_32 timeout_ms,
                          uint_32 client_capabilities)
{
        record(CALL_CONNECT);
        if (!string_equal(service, "compositor") ||
            timeout_ms != DESKTOP_TIMEOUT_MS || client_capabilities != 0)
                mismatch = 1;
        if (fail_call == CALL_CONNECT)
                return -ECONNREFUSED;
        context->fd = 4;
        context->connected = 1;
        return 0;
}

int_32 poudland_v1_window_create(
    struct poudland_v1_context *context,
    const struct poudland_v1_window_new *request, int_32 timeout_ms,
    struct poudland_v1_window_init *result)
{
        const struct expected_window *expected;

        record(CALL_CREATE);
        if (!context->connected || create_count >= 3)
                mismatch = 1;
        expected = &expected_windows[create_count < 3 ? create_count : 0];
        if (timeout_ms != DESKTOP_TIMEOUT_MS || request->x != expected->x ||
            request->y != expected->y || request->width != expected->width ||
            request->height != expected->height ||
            request->xrgb8888 != expected->color)
                mismatch = 1;
        create_count++;
        if (fail_call == CALL_CREATE && create_count == 2)
                return -EIO;
        result->window_id = zero_id && create_count == 3
                                ? 0
                                : duplicate_id && create_count == 3
                                      ? expected_windows[1].id
                                      : expected->id;
        result->x = expected->x;
        result->y = expected->y;
        result->width = expected->width;
        result->height = expected->height;
        result->xrgb8888 = expected->color;
        if (bad_echo && create_count == 3)
                result->height++;
        return 0;
}

int_32 poudland_v1_window_close(struct poudland_v1_context *context,
                                uint_32 window_id, int_32 timeout_ms)
{
        record(CALL_CLOSE);
        if (!context->connected || window_id != expected_windows[2].id ||
            timeout_ms != DESKTOP_TIMEOUT_MS)
                mismatch = 1;
        return fail_call == CALL_CLOSE ? -EIO : 0;
}

int_32 poudland_v1_disconnect(struct poudland_v1_context *context)
{
        record(CALL_DISCONNECT);
        disconnect_count++;
        context->fd = -1;
        context->connected = 0;
        return 0;
}

int_32 wait2(struct pollfd *fds, uint_32 count, int_32 timeout_ms)
{
        record(CALL_WAIT);
        if (fds != NULL || count != 0 || timeout_ms != 1000)
                mismatch = 1;
        wait_count++;
        return wait_count == 1 ? 0 : -EIO;
}

static int run_normal(void)
{
        char arg0[] = "desktop";
        char *argv[] = {arg0, NULL};

        return desktop_entry(1, argv);
}

static int success_sequence(void)
{
        static const uint_32 expected_calls[] = {
            CALL_INIT, CALL_CONNECT, CALL_CREATE, CALL_CREATE, CALL_CREATE,
            CALL_CLOSE, CALL_WAIT, CALL_WAIT, CALL_DISCONNECT,
        };
        uint_32 index;

        reset_test();
        if (run_normal() != 1 || mismatch || create_count != 3 ||
            wait_count != 2 || disconnect_count != 1 ||
            call_count != sizeof(expected_calls) / sizeof(expected_calls[0]))
                return 1;
        for (index = 0; index < call_count; ++index) {
                if (calls[index] != expected_calls[index])
                        return 2;
        }
        return 0;
}

static int failures_disconnect_and_stop(void)
{
        reset_test();
        fail_call = CALL_CONNECT;
        if (run_normal() != 1 || mismatch || create_count != 0 ||
            wait_count != 0 || disconnect_count != 1)
                return 10;

        reset_test();
        fail_call = CALL_CREATE;
        if (run_normal() != 1 || mismatch || create_count != 2 ||
            wait_count != 0 || disconnect_count != 1)
                return 11;

        reset_test();
        fail_call = CALL_CLOSE;
        if (run_normal() != 1 || mismatch || create_count != 3 ||
            wait_count != 0 || disconnect_count != 1)
                return 12;

        reset_test();
        bad_echo = true;
        if (run_normal() != 1 || mismatch || create_count != 3 ||
            wait_count != 0 || disconnect_count != 1)
                return 13;

        reset_test();
        duplicate_id = true;
        if (run_normal() != 1 || mismatch || create_count != 3 ||
            wait_count != 0 || disconnect_count != 1)
                return 14;

        reset_test();
        zero_id = true;
        if (run_normal() != 1 || mismatch || create_count != 3 ||
            wait_count != 0 || disconnect_count != 1)
                return 15;
        return 0;
}

static int exec_smoke_unchanged(void)
{
        char arg0[] = "desktop";
        char arg1[] = "--exec-smoke";
        char *argv[] = {arg0, arg1, NULL};

        reset_test();
        return desktop_entry(2, argv) == 43 && call_count == 0 ? 0 : 20;
}

int main(void)
{
        int result = success_sequence();

        if (result != 0)
                return result;
        result = failures_disconnect_and_stop();
        return result != 0 ? result : exec_smoke_unchanged();
}
