#include "poudland_p0.h"
#include "test.h"

#include <frog/test.h>
#include <frog/types.h>

static volatile uint_32 poudland_p0_data_cookie = 0x50304330U;
static struct poudland_p0_scene scene;

static void stop_with_cleanup(void) __attribute__((noreturn));

static void stop_with_cleanup(void)
{
        poudland_p0_scene_close_input(&scene);
        poudland_p0_cursor_release(&scene.cursor);
        poudland_p0_display_close(&scene.display);
        poudland_p0_test_finish();
}

int poudland_p0_main(void)
{
        bool data_segment_loaded =
            poudland_p0_data_cookie == 0x50304330U;

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
}
