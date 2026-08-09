#ifndef POUDLAND_P0_H
#define POUDLAND_P0_H

#include "geometry.h"

#include <frog/fb.h>
#include <frog/types.h>

#define POUDLAND_P0_WINDOW_COUNT 2U
#define POUDLAND_P0_CURSOR_LIMIT 64U

struct poudland_p0_protocol;

struct poudland_p0_window {
        struct poudland_p0_rect bounds;
        uint_32 color;
        uint_8 last_key;
};

struct poudland_p0_cursor {
        uint_32 width;
        uint_32 height;
        uint_32 *pixels;
};

struct poudland_p0_display {
        int_32 fd;
        struct frog_fb_info info;
        volatile uint_8 *framebuffer;
        uint_32 *backbuffer;
        uint_32 backbuffer_length;
        struct poudland_p0_rect damage;
        bool damaged;
        uint_32 frame_count;
        uint_32 presented_pixels;
        uint_32 damage_requests;
};

struct poudland_p0_scene {
        struct poudland_p0_display display;
        struct poudland_p0_window windows[POUDLAND_P0_WINDOW_COUNT];
        const struct poudland_p0_protocol *client_protocol;
        struct poudland_p0_cursor cursor;
        int_32 cursor_x;
        int_32 cursor_y;
        int_32 focused_window;
        int_32 dragged_window;
        int_32 drag_offset_x;
        int_32 drag_offset_y;
        uint_32 mouse_buttons;
        int_32 keyboard_fd;
        int_32 mouse_fd;
        uint_64 last_present_ms;
};

int_32 poudland_p0_display_open(struct poudland_p0_display *display);
void poudland_p0_display_close(struct poudland_p0_display *display);
bool poudland_p0_display_info_valid(const struct frog_fb_info *info);
uint_32 poudland_p0_pack_pixel(const struct frog_fb_info *info,
                              uint_32 xrgb);
bool poudland_p0_format_self_test(void);

bool poudland_p0_rect_clip(struct poudland_p0_rect *rect,
                           uint_32 width, uint_32 height);
void poudland_p0_damage(struct poudland_p0_display *display,
                        struct poudland_p0_rect rect);
bool poudland_p0_render(struct poudland_p0_scene *scene);

void poudland_p0_scene_prepare(struct poudland_p0_scene *scene);
int_32 poudland_p0_scene_open_input(struct poudland_p0_scene *scene);
void poudland_p0_scene_close_input(struct poudland_p0_scene *scene);
bool poudland_p0_scene_run(struct poudland_p0_scene *scene);
bool poudland_p0_scene_present(struct poudland_p0_scene *scene);

int_32 poudland_p0_bmp_load_cursor(const char *path,
                                   struct poudland_p0_cursor *cursor);
void poudland_p0_cursor_release(struct poudland_p0_cursor *cursor);

#endif
