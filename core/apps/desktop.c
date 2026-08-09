#include <frog/syscall.h>
#include <frog/types.h>
#include <frog/errno.h>
#include <frog/graphical_startup.h>
#include <frog/test.h>
#include <gua/poudland_v1.h>

#define DESKTOP_EXEC_SMOKE_STATUS 43
#define DESKTOP_REQUEST_TIMEOUT_MS 2000
#define DESKTOP_EVENT_TIMEOUT_MS   1000

static volatile uint_32 desktop_data_cookie = 0x44534b31U;
static volatile uint_32 desktop_bss_cookie;
static struct poudland_v1_context desktop_poudland;
struct desktop_window_state {
        int_32 x;
        int_32 y;
        uint_32 keycode;
        uint_32 key_action;
        uint_32 key_modifiers;
        uint_32 codepoint;
};

#ifdef FROG_DESKTOP_SMOKE_TEST
struct desktop_test_observation {
        bool pointer_down;
        bool keyboard;
        bool configured;
        bool reported;
};

static int_32 desktop_test_syscall2(uint_32 number, uint_32 first,
                                    uint_32 second)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(number), "b"(first), "c"(second)
                         : "memory");
        return result;
}

static void desktop_test_report(uint_32 id, bool passed)
{
        (void) desktop_test_syscall2(SYS_TEST_REPORT, id, passed);
}

static int_32 desktop_test_sync(uint_32 command)
{
        int_32 result;

        __asm__ volatile("int $0x93"
                         : "=a"(result)
                         : "a"(SYS_TEST_SYNC), "b"(command)
                         : "memory");
        return result;
}

static bool desktop_test_observe(
    const struct poudland_v1_message *event,
    const uint_32 window_ids[3], struct desktop_test_observation *observed)
{
        if (event->header.type == POUDLAND_V1_MSG_POINTER_EVENT) {
                const struct poudland_v1_pointer_event *pointer =
                    (const struct poudland_v1_pointer_event *) event->payload;

                if (pointer->type == POUDLAND_V1_POINTER_DOWN) {
                        if (pointer->window_id != window_ids[1] ||
                            pointer->screen_x != 230 ||
                            pointer->screen_y != 210 ||
                            pointer->buttons != 1U)
                                return false;
                        observed->pointer_down = true;
                }
        } else if (event->header.type == POUDLAND_V1_MSG_KEY_EVENT) {
                const struct poudland_v1_key_event *key =
                    (const struct poudland_v1_key_event *) event->payload;

                if (key->window_id != window_ids[1] ||
                    key->keycode != 'a' ||
                    key->action != POUDLAND_V1_KEY_PRESS ||
                    key->codepoint != 'a')
                        return false;
                observed->keyboard = true;
        } else if (event->header.type ==
                   POUDLAND_V1_MSG_WINDOW_CONFIGURE) {
                const struct poudland_v1_window_configure *configure =
                    (const struct poudland_v1_window_configure *)
                        event->payload;

                if (configure->window_id == window_ids[1] &&
                    configure->x == 260 && configure->y == 225)
                        observed->configured = true;
        }
        if (!observed->reported && observed->pointer_down &&
            observed->keyboard && observed->configured) {
                desktop_test_report(FROG_TEST_DESKTOP_CLIENT_OBSERVED,
                                    true);
                if (desktop_test_sync(FROG_TEST_DESKTOP_CLIENT_READY) != 0)
                        return false;
                observed->reported = true;
        }
        return true;
}
#endif

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

static int_32 known_window_index(const uint_32 window_ids[3],
                                 uint_32 window_id)
{
        uint_32 index;

        for (index = 0; index < 3; ++index) {
                if (window_ids[index] == window_id)
                        return (int_32) index;
        }
        return -1;
}

static bool desktop_event_apply(
    const struct poudland_v1_message *event,
    const uint_32 window_ids[3],
    struct desktop_window_state windows[2])
{
        if (!event || event->header.request_id != 0)
                return false;
        if (event->header.type == POUDLAND_V1_MSG_POINTER_EVENT) {
                const struct poudland_v1_pointer_event *pointer =
                    (const struct poudland_v1_pointer_event *) event->payload;

                return event->header.payload_size == sizeof(*pointer) &&
                       known_window_index(window_ids,
                                          pointer->window_id) >= 0 &&
                       pointer->type >= POUDLAND_V1_POINTER_CLICK &&
                       pointer->type <= POUDLAND_V1_POINTER_DRAG;
        }
        if (event->header.type == POUDLAND_V1_MSG_WINDOW_CONFIGURE) {
                const struct poudland_v1_window_configure *configure =
                    (const struct poudland_v1_window_configure *)
                        event->payload;
                int_32 index = known_window_index(
                    window_ids, configure->window_id);

                if (event->header.payload_size != sizeof(*configure) ||
                    index < 0)
                        return false;
                if (index < 2) {
                        windows[index].x = configure->x;
                        windows[index].y = configure->y;
                }
                return true;
        }
        if (event->header.type == POUDLAND_V1_MSG_KEY_EVENT) {
                const struct poudland_v1_key_event *key =
                    (const struct poudland_v1_key_event *) event->payload;
                int_32 index = known_window_index(window_ids,
                                                  key->window_id);

                if (event->header.payload_size != sizeof(*key) ||
                    index < 0 || key->action < POUDLAND_V1_KEY_PRESS ||
                    key->action > POUDLAND_V1_KEY_REPEAT)
                        return false;
                if (index < 2) {
                        windows[index].keycode = key->keycode;
                        windows[index].key_action = key->action;
                        windows[index].key_modifiers = key->modifiers;
                        windows[index].codepoint = key->codepoint;
                }
                return true;
        }
        return false;
}

static int desktop_failure_status(int_32 status, bool connecting)
{
        if (connecting && status == -ETIMEDOUT)
                return FROG_DESKTOP_EXIT_CONNECT_TIMEOUT;
        if (status == -ECONNRESET)
                return FROG_DESKTOP_EXIT_COMPOSITOR_HUP;
        return FROG_DESKTOP_EXIT_RUNTIME_FAILURE;
}

static int run_desktop(void)
{
        uint_32 window_ids[3];
        struct desktop_window_state window_states[2] = {0};
        uint_32 index;
        int_32 status;
        int result = FROG_DESKTOP_EXIT_RUNTIME_FAILURE;
#ifdef FROG_DESKTOP_SMOKE_TEST
        struct desktop_test_observation observed = {0};
#endif

        poudland_v1_context_init(&desktop_poudland);
        status = poudland_v1_connect(
            &desktop_poudland, "compositor", DESKTOP_REQUEST_TIMEOUT_MS,
            POUDLAND_V1_CAP_CONFIGURE | POUDLAND_V1_CAP_POINTER |
                POUDLAND_V1_CAP_KEYBOARD);
        if (status != 0) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                desktop_test_report(FROG_TEST_DESKTOP_CLIENT_CONNECT, false);
#endif
                result = desktop_failure_status(status, true);
                goto failure;
        }
#ifdef FROG_DESKTOP_SMOKE_TEST
        desktop_test_report(FROG_TEST_DESKTOP_CLIENT_CONNECT, true);
#endif
        for (index = 0; index < 3; ++index) {
                struct poudland_v1_window_init initialized;
                uint_32 previous;

                status = poudland_v1_window_create(
                    &desktop_poudland, &desktop_windows[index],
                    DESKTOP_REQUEST_TIMEOUT_MS, &initialized);
                if (status != 0) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                        desktop_test_report(
                            FROG_TEST_DESKTOP_THREE_CREATES, false);
#endif
                        result = desktop_failure_status(status, false);
                        goto failure;
                }
                if (!window_init_matches(&desktop_windows[index],
                                         &initialized)) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                        desktop_test_report(
                            FROG_TEST_DESKTOP_THREE_CREATES, false);
#endif
                        goto failure;
                }
                for (previous = 0; previous < index; ++previous) {
                        if (window_ids[previous] == initialized.window_id) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                                desktop_test_report(
                                    FROG_TEST_DESKTOP_THREE_CREATES, false);
#endif
                                goto failure;
                        }
                }
                window_ids[index] = initialized.window_id;
                if (index < 2) {
                        window_states[index].x = initialized.x;
                        window_states[index].y = initialized.y;
                }
        }
#ifdef FROG_DESKTOP_SMOKE_TEST
        desktop_test_report(FROG_TEST_DESKTOP_THREE_CREATES, true);
#endif
        status = poudland_v1_window_close(
            &desktop_poudland, window_ids[2], DESKTOP_REQUEST_TIMEOUT_MS);
        if (status != 0) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                desktop_test_report(FROG_TEST_DESKTOP_THIRD_CLOSE, false);
#endif
                result = desktop_failure_status(status, false);
                goto failure;
        }
#ifdef FROG_DESKTOP_SMOKE_TEST
        desktop_test_report(FROG_TEST_DESKTOP_THIRD_CLOSE, true);
#endif
        for (;;) {
                struct poudland_v1_message event;

                status = poudland_v1_next_event(
                    &desktop_poudland, DESKTOP_EVENT_TIMEOUT_MS, &event);
                if (status == -ETIMEDOUT)
                        continue;
                if (status != 0) {
                        result = desktop_failure_status(status, false);
                        goto failure;
                }
                if (!desktop_event_apply(&event, window_ids,
                                         window_states))
                        goto failure;
#ifdef FROG_DESKTOP_SMOKE_TEST
                if (!desktop_test_observe(&event, window_ids, &observed)) {
                        desktop_test_report(
                            FROG_TEST_DESKTOP_CLIENT_OBSERVED, false);
                        goto failure;
                }
#endif
        }

failure:
        (void) poudland_v1_disconnect(&desktop_poudland);
        return result;
}

int main(int argc, char **argv)
{
        if (exec_smoke_requested(argc, argv)) {
                return desktop_data_cookie == 0x44534b31U &&
                       desktop_bss_cookie == 0
                           ? DESKTOP_EXEC_SMOKE_STATUS
                           : 1;
        }

#ifdef FROG_DESKTOP_SMOKE_TEST
        desktop_test_report(FROG_TEST_DESKTOP_CLIENT_EXEC,
                            desktop_data_cookie == 0x44534b31U &&
                            desktop_bss_cookie == 0);
#endif

        return run_desktop();
}
