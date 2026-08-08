#include "poudland_p0.h"
#include "test.h"

#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/test.h>
#include <frog/time.h>
#include <input/mouse.h>

#define POUDLAND_P0_FRAME_MS 17U
#define POUDLAND_P0_PHASE_TIMEOUT_MS 3000U
#define POUDLAND_P0_IDLE_MS 500U

static bool monotonic_ms(uint_64 *milliseconds)
{
        struct timespec now;

        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
            now.tv_sec < 0 || now.tv_nsec < 0 ||
            now.tv_nsec >= 1000000000)
                return false;
        *milliseconds = (uint_64) now.tv_sec * 1000U +
                        (uint_32) now.tv_nsec / 1000000U;
        return true;
}

static int_32 remaining_ms(uint_64 deadline)
{
        uint_64 now;
        uint_64 remaining;

        if (!monotonic_ms(&now))
                return -1;
        if (now >= deadline)
                return 0;
        remaining = deadline - now;
        return remaining > 0x7fffffffU ? 0x7fffffff : (int_32) remaining;
}

static bool deadline_after(uint_32 milliseconds, uint_64 *deadline)
{
        uint_64 now;

        if (!monotonic_ms(&now))
                return false;
        *deadline = now + milliseconds;
        return true;
}

static void prepare_descriptors(struct poudland_p0_scene *scene,
                                struct pollfd descriptors[2])
{
        descriptors[0].fd = scene->keyboard_fd;
        descriptors[0].events = POLLIN;
        descriptors[0].revents = 0;
        descriptors[1].fd = scene->mouse_fd;
        descriptors[1].events = POLLIN;
        descriptors[1].revents = 0;
}

static int_32 hit_test(const struct poudland_p0_scene *scene,
                       int_32 x, int_32 y)
{
        int_32 index;

        for (index = (int_32) POUDLAND_P0_WINDOW_COUNT - 1;
             index >= 0; --index) {
                const struct poudland_p0_rect *bounds =
                    &scene->windows[index].bounds;

                if (x >= bounds->x && y >= bounds->y &&
                    x < bounds->x + bounds->width &&
                    y < bounds->y + bounds->height)
                        return index;
        }
        return -1;
}

static int_32 clamp_position(int_32 value, int_32 maximum)
{
        if (value < 0)
                return 0;
        if (value > maximum)
                return maximum;
        return value;
}

static bool handle_mouse_packet(struct poudland_p0_scene *scene,
                                const mouse_device_packet_t *packet)
{
        struct poudland_p0_rect old_cursor = {
            .x = scene->cursor_x,
            .y = scene->cursor_y,
            .width = (int_32) scene->cursor.width,
            .height = (int_32) scene->cursor.height,
        };
        bool was_left = (scene->mouse_buttons & LEFT_CLICK) != 0;
        bool is_left = (packet->buttons & LEFT_CLICK) != 0;
        bool moved;

        if (packet->magic != MOUSE_MAGIC)
                return false;
        if (packet->x_difference != 0 || packet->y_difference != 0) {
                int_32 next_x = clamp_position(
                    scene->cursor_x + packet->x_difference,
                    (int_32) scene->display.info.width - 1);
                int_32 next_y = clamp_position(
                    scene->cursor_y - packet->y_difference,
                    (int_32) scene->display.info.height - 1);

                moved = next_x != scene->cursor_x ||
                        next_y != scene->cursor_y;
                scene->cursor_x = next_x;
                scene->cursor_y = next_y;
        } else {
                moved = false;
        }
        if (moved) {
                poudland_p0_damage(&scene->display, old_cursor);
                if (scene->dragged_window >= 0 && (was_left || is_left)) {
                        struct poudland_p0_rect *bounds =
                            &scene->windows[scene->dragged_window].bounds;
                        struct poudland_p0_rect old_bounds = *bounds;

                        bounds->x = clamp_position(
                            scene->cursor_x - scene->drag_offset_x,
                            (int_32) scene->display.info.width -
                                bounds->width);
                        bounds->y = clamp_position(
                            scene->cursor_y - scene->drag_offset_y,
                            (int_32) scene->display.info.height -
                                bounds->height);
                        if (bounds->x != old_bounds.x ||
                            bounds->y != old_bounds.y) {
                                poudland_p0_damage(&scene->display,
                                                   old_bounds);
                                poudland_p0_damage(&scene->display, *bounds);
                        }
                }
                poudland_p0_damage(&scene->display,
                    (struct poudland_p0_rect) {
                        .x = scene->cursor_x,
                        .y = scene->cursor_y,
                        .width = (int_32) scene->cursor.width,
                        .height = (int_32) scene->cursor.height,
                    });
        }
        if (!was_left && is_left) {
                int_32 hit = hit_test(scene, scene->cursor_x,
                                      scene->cursor_y);

                if (hit != scene->focused_window) {
                        if (scene->focused_window >= 0)
                                poudland_p0_damage(
                                    &scene->display,
                                    scene->windows[scene->focused_window].bounds);
                        scene->focused_window = hit;
                        if (hit >= 0)
                                poudland_p0_damage(
                                    &scene->display,
                                    scene->windows[hit].bounds);
                }
                scene->dragged_window = hit;
                if (hit >= 0) {
                        scene->drag_offset_x = scene->cursor_x -
                            scene->windows[hit].bounds.x;
                        scene->drag_offset_y = scene->cursor_y -
                            scene->windows[hit].bounds.y;
                }
        } else if (was_left && !is_left) {
                scene->dragged_window = -1;
        }
        scene->mouse_buttons = packet->buttons;
        return true;
}

static bool drain_keyboard(struct poudland_p0_scene *scene)
{
        for (;;) {
                uint_8 key;
                int_32 result = read(scene->keyboard_fd, &key, sizeof(key));

                if (result == -EAGAIN)
                        return true;
                if (result != sizeof(key))
                        return false;
                if (scene->focused_window >= 0)
                        scene->windows[scene->focused_window].last_key = key;
        }
}

static bool drain_mouse(struct poudland_p0_scene *scene)
{
        for (;;) {
                mouse_device_packet_t packet;
                int_32 result = read(scene->mouse_fd, &packet,
                                     sizeof(packet));

                if (result == -EAGAIN)
                        return true;
                if (result != sizeof(packet) ||
                    !handle_mouse_packet(scene, &packet))
                        return false;
        }
}

static bool process_ready(struct poudland_p0_scene *scene,
                          struct pollfd descriptors[2])
{
        uint_16 errors = POLLERR | POLLHUP | POLLNVAL;

        if ((descriptors[0].revents & errors) != 0 ||
            (descriptors[1].revents & errors) != 0)
                return false;
        if ((descriptors[0].revents & POLLIN) != 0 &&
            !drain_keyboard(scene))
                return false;
        if ((descriptors[1].revents & POLLIN) != 0 &&
            !drain_mouse(scene))
                return false;
        return true;
}

static bool pace_and_present(struct poudland_p0_scene *scene)
{
        uint_64 target;
        int_32 delay;

        if (!scene->display.damaged)
                return true;
        target = scene->last_present_ms + POUDLAND_P0_FRAME_MS;
        delay = remaining_ms(target);
        if (delay < 0 || (delay > 0 && wait2(NULL, 0, delay) != 0))
                return false;
        if (!poudland_p0_render(scene) ||
            !monotonic_ms(&scene->last_present_ms))
                return false;
        return true;
}

static int_32 wait_for_events_until(struct poudland_p0_scene *scene,
                                    uint_64 deadline)
{
        struct pollfd descriptors[2];

        for (;;) {
                int_32 timeout = remaining_ms(deadline);
                int_32 result;

                if (timeout <= 0)
                        return timeout;
                prepare_descriptors(scene, descriptors);
                result = wait2(descriptors, 2, timeout);
                if (result < 0)
                        return -1;
                if (result == 0)
                        continue;
                return process_ready(scene, descriptors) &&
                       pace_and_present(scene) ? 1 : -1;
        }
}

#ifdef FROG_POUDLAND_P0_TEST
static bool idle_quiescent(struct poudland_p0_scene *scene,
                           uint_32 duration_ms)
{
        struct pollfd descriptors[2];
        uint_64 now;
        uint_64 deadline;
        uint_32 frame_count = scene->display.frame_count;
        uint_32 presented_pixels = scene->display.presented_pixels;

        if (!monotonic_ms(&now))
                return false;
        deadline = now + duration_ms;
        for (;;) {
                int_32 timeout = remaining_ms(deadline);
                int_32 result;

                if (timeout < 0)
                        return false;
                if (timeout == 0)
                        return scene->display.frame_count == frame_count &&
                               scene->display.presented_pixels ==
                                   presented_pixels &&
                               !scene->display.damaged;
                prepare_descriptors(scene, descriptors);
                result = wait2(descriptors, 2, timeout);
                if (result != 0)
                        return false;
        }
}
#endif

void poudland_p0_scene_prepare(struct poudland_p0_scene *scene)
{
        scene->windows[0].bounds = (struct poudland_p0_rect) {
            .x = 100, .y = 100, .width = 200, .height = 160,
        };
        scene->windows[0].color = 0x00cc5533U;
        scene->windows[1].bounds = (struct poudland_p0_rect) {
            .x = 220, .y = 200, .width = 320, .height = 240,
        };
        scene->windows[1].color = 0x00339966U;
        scene->cursor_x = 230;
        scene->cursor_y = 210;
        scene->focused_window = -1;
        scene->dragged_window = -1;
        scene->keyboard_fd = -1;
        scene->mouse_fd = -1;
}

int_32 poudland_p0_scene_open_input(struct poudland_p0_scene *scene)
{
        scene->keyboard_fd = open("/dev/input/event0",
                                  O_RDONLY | O_NONBLOCK);
        if (scene->keyboard_fd < 0)
                return scene->keyboard_fd;
        scene->mouse_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
        if (scene->mouse_fd < 0) {
                int_32 result = scene->mouse_fd;

                poudland_p0_scene_close_input(scene);
                return result;
        }
        return 0;
}

void poudland_p0_scene_close_input(struct poudland_p0_scene *scene)
{
        if (scene->keyboard_fd >= 0)
                (void) close(scene->keyboard_fd);
        if (scene->mouse_fd >= 0)
                (void) close(scene->mouse_fd);
        scene->keyboard_fd = -1;
        scene->mouse_fd = -1;
}

bool poudland_p0_scene_run(struct poudland_p0_scene *scene)
{
        if (!monotonic_ms(&scene->last_present_ms))
                return false;
#ifdef FROG_POUDLAND_P0_TEST
        bool passed = idle_quiescent(scene, POUDLAND_P0_IDLE_MS);
        uint_64 deadline;

        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_IDLE, passed);
        if (!passed || poudland_p0_test_sync(
                FROG_TEST_POUDLAND_BUILTIN_INITIAL_READY) != 0)
                return false;
        if (!deadline_after(POUDLAND_P0_PHASE_TIMEOUT_MS, &deadline))
                return false;
        while (scene->focused_window != 1 ||
               (scene->mouse_buttons & LEFT_CLICK) == 0) {
                if (wait_for_events_until(scene, deadline) != 1)
                        return false;
        }
        passed = scene->display.frame_count == 2U &&
                 scene->display.presented_pixels ==
                    scene->display.info.width * scene->display.info.height +
                    320U * 240U &&
                 scene->display.damage_requests == 2U;
        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_FOCUS, passed);
        if (!passed || poudland_p0_test_sync(
                FROG_TEST_POUDLAND_BUILTIN_FOCUS_READY) != 0)
                return false;
        if (!deadline_after(POUDLAND_P0_PHASE_TIMEOUT_MS, &deadline))
                return false;
        while (scene->windows[1].last_key != 'a') {
                if (wait_for_events_until(scene, deadline) != 1)
                        return false;
        }
        passed = scene->focused_window == 1 &&
                 scene->windows[1].last_key == 'a' &&
                 scene->windows[0].last_key == 0;
        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_KEYBOARD, passed);
        if (!passed || poudland_p0_test_sync(
                FROG_TEST_POUDLAND_BUILTIN_KEYBOARD_READY) != 0)
                return false;
        if (!deadline_after(POUDLAND_P0_PHASE_TIMEOUT_MS, &deadline))
                return false;
        while (scene->windows[1].bounds.x != 260 ||
               scene->windows[1].bounds.y != 225) {
                if (wait_for_events_until(scene, deadline) != 1)
                        return false;
        }
        passed = scene->cursor_x == 270 && scene->cursor_y == 235 &&
                 scene->dragged_window == 1 &&
                 scene->display.frame_count == 3U;
        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_DRAG, passed);
        if (!passed || poudland_p0_test_sync(
                FROG_TEST_POUDLAND_BUILTIN_DRAG_READY) != 0)
                return false;
        if (!deadline_after(POUDLAND_P0_PHASE_TIMEOUT_MS, &deadline))
                return false;
        while ((scene->mouse_buttons & LEFT_CLICK) != 0) {
                if (wait_for_events_until(scene, deadline) != 1)
                        return false;
        }
        passed = idle_quiescent(scene, POUDLAND_P0_IDLE_MS) &&
                 scene->display.frame_count == 3U &&
                 scene->display.presented_pixels ==
                    scene->display.info.width * scene->display.info.height +
                    320U * 240U + 360U * 265U &&
                 scene->display.damage_requests == 6U &&
                 scene->focused_window == 1 &&
                 scene->dragged_window == -1 &&
                 scene->windows[1].bounds.x == 260 &&
                 scene->windows[1].bounds.y == 225;
        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_FINAL_DAMAGE,
                                passed);
        if (!passed)
                return false;
        return poudland_p0_test_sync(
            FROG_TEST_POUDLAND_BUILTIN_FINAL_READY) == 0;
#else
        for (;;) {
                uint_64 deadline;
                int_32 result;

                if (!deadline_after(1000U, &deadline))
                        return false;
                result = wait_for_events_until(scene, deadline);
                if (result < 0)
                        return false;
        }
#endif
}
