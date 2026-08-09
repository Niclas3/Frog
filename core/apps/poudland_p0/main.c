#include "poudland_p0.h"
#include "server.h"
#include "test.h"

#include <frog/test.h>
#include <frog/types.h>

static volatile uint_32 poudland_p0_data_cookie = 0x50304330U;
static struct poudland_p0_scene scene;
#ifndef FROG_POUDLAND_P0_TEST
static struct poudland_p0_server server;
#endif

#define POUDLAND_P0_EXEC_SMOKE_STATUS 42

#ifdef FROG_POUDLAND_P0_TEST
static void stop_with_cleanup(void) __attribute__((noreturn));

static void stop_with_cleanup(void)
{
        poudland_p0_scene_close_input(&scene);
        poudland_p0_cursor_release(&scene.cursor);
        poudland_p0_display_close(&scene.display);
        poudland_p0_test_finish();
}
#endif

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

#ifndef FROG_POUDLAND_P0_TEST
static int run_production(void)
{
        int_32 status;

        if (!poudland_p0_format_self_test()) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                poudland_p0_test_report(
                    FROG_TEST_DESKTOP_COMPOSITOR_READY, false);
#endif
                return 1;
        }
        poudland_p0_scene_prepare(&scene);
        status = poudland_p0_display_open(&scene.display);
        if (status != 0) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                poudland_p0_test_report(
                    FROG_TEST_DESKTOP_COMPOSITOR_READY, false);
#endif
                return 1;
        }
        status = poudland_p0_bmp_load_cursor("/test/b.bmp", &scene.cursor);
        if (status != 0) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                poudland_p0_test_report(
                    FROG_TEST_DESKTOP_COMPOSITOR_READY, false);
#endif
                poudland_p0_display_close(&scene.display);
                return 1;
        }
        status = poudland_p0_server_open(&server, &scene);
        if (status != 0) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                poudland_p0_test_report(
                    FROG_TEST_DESKTOP_SERVICE_BIND, false);
                poudland_p0_test_report(
                    FROG_TEST_DESKTOP_COMPOSITOR_READY, false);
#endif
                poudland_p0_cursor_release(&scene.cursor);
                poudland_p0_display_close(&scene.display);
                return 1;
        }
#ifdef FROG_DESKTOP_SMOKE_TEST
        poudland_p0_test_report(FROG_TEST_DESKTOP_SERVICE_BIND, true);
#endif
        status = poudland_p0_scene_open_input(&scene);
        if (status != 0) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                poudland_p0_test_report(
                    FROG_TEST_DESKTOP_COMPOSITOR_READY, false);
#endif
                poudland_p0_server_close(&server);
                poudland_p0_cursor_release(&scene.cursor);
                poudland_p0_display_close(&scene.display);
                return 1;
        }
        poudland_p0_damage(&scene.display,
            (struct poudland_p0_rect) {
                .x = 0,
                .y = 0,
                .width = (int_32) scene.display.info.width,
                .height = (int_32) scene.display.info.height,
            });
        if (!poudland_p0_scene_present(&scene)) {
#ifdef FROG_DESKTOP_SMOKE_TEST
                poudland_p0_test_report(
                    FROG_TEST_DESKTOP_COMPOSITOR_READY, false);
#endif
                poudland_p0_scene_close_input(&scene);
                poudland_p0_server_close(&server);
                poudland_p0_cursor_release(&scene.cursor);
                poudland_p0_display_close(&scene.display);
                return 1;
        }
#ifdef FROG_DESKTOP_SMOKE_TEST
        poudland_p0_test_report(FROG_TEST_DESKTOP_COMPOSITOR_READY, true);
#endif
        if (!poudland_p0_server_run(&server, &scene)) {
                poudland_p0_scene_close_input(&scene);
                poudland_p0_server_close(&server);
                poudland_p0_cursor_release(&scene.cursor);
                poudland_p0_display_close(&scene.display);
                return 1;
        }
        return 0;
}
#endif

int main(int argc, char **argv)
{
        bool data_segment_loaded =
            poudland_p0_data_cookie == 0x50304330U;

        if (argc == 2 && argv && argv[0] &&
            string_equal(argv[1], "--exec-smoke"))
                return data_segment_loaded ? POUDLAND_P0_EXEC_SMOKE_STATUS : 1;

#ifndef FROG_POUDLAND_P0_TEST
#ifdef FROG_DESKTOP_SMOKE_TEST
        poudland_p0_test_report(FROG_TEST_DESKTOP_COMPOSITOR_EXEC,
                                data_segment_loaded);
#endif
        if (!data_segment_loaded)
                return 1;
        return run_production();
#else

        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_EXEC,
                                data_segment_loaded);
        if (!data_segment_loaded)
                poudland_p0_test_finish();

        bool passed = poudland_p0_format_self_test();

        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_FORMATS, passed);
        if (!passed)
                poudland_p0_test_finish();

        poudland_p0_scene_prepare(&scene);
        passed = poudland_p0_display_open(&scene.display) == 0;
        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_FRAMEBUFFER,
                                passed);
        if (!passed)
                stop_with_cleanup();

        passed = poudland_p0_bmp_load_cursor("/test/b.bmp",
                                             &scene.cursor) == 0 &&
                 scene.cursor.width == 48U && scene.cursor.height == 48U;
        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_BMP_CURSOR,
                                passed);
        if (!passed)
                stop_with_cleanup();

        passed = poudland_p0_scene_open_input(&scene) == 0;
        if (!passed) {
                poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_FOCUS,
                                        false);
                stop_with_cleanup();
        }
        poudland_p0_damage(&scene.display,
            (struct poudland_p0_rect) {
                .x = 0,
                .y = 0,
                .width = (int_32) scene.display.info.width,
                .height = (int_32) scene.display.info.height,
            });
        passed = poudland_p0_render(&scene) &&
                 scene.display.frame_count == 1U &&
                 scene.display.presented_pixels ==
                    scene.display.info.width * scene.display.info.height &&
                 scene.display.damage_requests == 1U;
        poudland_p0_test_report(FROG_TEST_POUDLAND_BUILTIN_INITIAL_DAMAGE,
                                passed);
        if (!passed)
                stop_with_cleanup();

        if (!poudland_p0_scene_run(&scene)) {
                poudland_p0_test_report(
                    FROG_TEST_POUDLAND_BUILTIN_FINAL_DAMAGE, false);
                stop_with_cleanup();
        }
        stop_with_cleanup();
#endif
}
