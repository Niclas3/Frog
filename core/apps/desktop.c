#include <frog/syscall.h>
#include <frog/types.h>
#include <gua/poudland_v1.h>

#define DESKTOP_EXEC_SMOKE_STATUS 43
#define DESKTOP_REQUEST_TIMEOUT_MS 2000
#define DESKTOP_HOLD_INTERVAL_MS   1000

static volatile uint_32 desktop_data_cookie = 0x44534b31U;
static volatile uint_32 desktop_bss_cookie;
static struct poudland_v1_context desktop_poudland;
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

static bool string_equal(const char *left, const char *right)
{
        if (!left || !right)
                return false;
        while (*left && *right) {
                if (*left++ != *right++)
                        return false;
        }
        return *left == *right;
}

static bool exec_smoke_requested(int argc, char **argv)
{
        return argc == 2 && argv && argv[0] &&
               string_equal(argv[1], "--exec-smoke");
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

static int run_desktop(void)
{
        uint_32 window_ids[3];
        uint_32 index;
        int_32 status;

        poudland_v1_context_init(&desktop_poudland);
        status = poudland_v1_connect(
            &desktop_poudland, "compositor", DESKTOP_REQUEST_TIMEOUT_MS, 0);
        if (status != 0)
                goto failure;
        for (index = 0; index < 3; ++index) {
                struct poudland_v1_window_init initialized;
                uint_32 previous;

                status = poudland_v1_window_create(
                    &desktop_poudland, &desktop_windows[index],
                    DESKTOP_REQUEST_TIMEOUT_MS, &initialized);
                if (status != 0 ||
                    !window_init_matches(&desktop_windows[index],
                                         &initialized))
                        goto failure;
                for (previous = 0; previous < index; ++previous) {
                        if (window_ids[previous] == initialized.window_id)
                                goto failure;
                }
                window_ids[index] = initialized.window_id;
        }
        status = poudland_v1_window_close(
            &desktop_poudland, window_ids[2], DESKTOP_REQUEST_TIMEOUT_MS);
        if (status != 0)
                goto failure;
        for (;;) {
                status = wait2(NULL, 0, DESKTOP_HOLD_INTERVAL_MS);
                if (status != 0)
                        goto failure;
        }

failure:
        (void) poudland_v1_disconnect(&desktop_poudland);
        return 1;
}

int main(int argc, char **argv)
{
        if (exec_smoke_requested(argc, argv)) {
                return desktop_data_cookie == 0x44534b31U &&
                       desktop_bss_cookie == 0
                           ? DESKTOP_EXEC_SMOKE_STATUS
                           : 1;
        }

        return run_desktop();
}
