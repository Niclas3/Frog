#include <device/console.h>  // for ioctl(port_num)
#include <gua/2d_graphics.h>
#include <gua/poudland-server.h>
#include <hid/mouse.h>
#include <list.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

#include <device/console.h>
#include "bmp.h"

static uint_32 poudland_current_time(poudland_globals_t *pg)
{
    struct timeval t;
    gettimeofday(&t, NULL);

    time_t sec_diff = t.tv_sec - pg->start_time;
    suseconds_t usec_diff = t.tv_usec - pg->start_subtime;

    if (t.tv_usec < pg->start_subtime) {
        sec_diff -= 1;
        usec_diff = (1000000 + t.tv_usec) - pg->start_subtime;
    }

    return (uint_32) (sec_diff * 1000 + usec_diff / 1000);
}

static uint_32 poudland_time_since(poudland_globals_t *pg, uint_32 start_time)
{
    uint_32 now = poudland_current_time(pg);
    uint_32 diff = now - start_time; /* Milliseconds */

    return diff;
}

/**
 * Convert window point to screen point
 *
 *****************************************************************************/
void convert_window_point(poudland_server_window_t *w,
                          int_32 x,
                          int_32 y,
                          int_32 *out_x,
                          int_32 *out_y)
{
}

void convert_screen_point(poudland_server_window_t *w,
                          int_32 x,
                          int_32 y,
                          int_32 *out_x,
                          int_32 *out_y)
{
}

bool point_in_rect(point_t point, rect_t rect)
{
    return true;
}

static bool in_window(poudland_server_window_t *w, point_t point)
{
    int x_max = w->x + w->width;
    int y_max = w->y + w->height;

    if ((point.X > x_max || point.X < w->x) ||
        (point.Y > y_max || point.Y < w->y)) {
        return false;
    } else {
        return true;
    }
}


/**
 * Create a server window object.
 *
 * Initializes a window of the particular size for a given client.
 */
static poudland_server_window_t *server_window_create(poudland_globals_t *pg,
                                                      int width,
                                                      int height,
                                                      void *owner,
                                                      uint_32 flags)
{
    poudland_server_window_t *win = malloc(sizeof(poudland_server_window_t));

    /* win->wid = next_wid(); */
    /* win->owner = owner; */
    list_add_tail(&win->server_w_target, &pg->windows);
    /* hashmap_set(yg->wids_to_windows, (void *) (uintptr_t) win->wid, win); */
    /* list_t *client_list = hashmap_get(yg->clients_to_windows, (void *)
     * owner); */
    /* list_insert(client_list, win); */

    win->x = 0;
    win->y = 0;
    win->z = 1;
    win->width = width;
    win->height = height;
    /* win->bufid = next_buf_id(); */
    win->rotation = 0;
    /* win->newbufid = 0; */
    win->client_flags = 0;
    win->client_icon = 0;
    win->client_length = 0;
    win->client_strings = NULL;
    /* win->anim_mode = 0; */
    /* win->anim_start = 0; */
    win->alpha_threshold = 0;
    win->show_mouse = 1;
    win->tiled = 0;
    win->untiled_width = 0;
    win->untiled_height = 0;
    win->default_mouse = 1;
    win->server_flags = flags;
    win->opacity = 255;
    win->hidden = 1;
    win->minimized = 0;

    uint_32 size = (width * height * sizeof(uint_32));
    win->buffer = malloc(size);
    memset(win->buffer, 0, size);

    // add to mid_zs
    list_add_tail(&win->server_w_mid_target, &pg->mid_zs);

    return win;
}
static void draw_mouse(poudland_globals_t *pg,
                       uint_32 pos_x,
                       uint_32 pos_y,
                       argb_t *color)
{
    gfx_context_t *ctx = pg->backend_ctx;
    point_t size = {.X = MOUSE_WIDTH, .Y = MOUSE_HEIGHT};
    point_t topleft = {.X = pos_x, .Y = pos_y};
    point_t downright = {.X = pos_x + size.X, .Y = pos_y + size.Y};
    argb_t defalut_cursor_color = FSK_SANDY_BROWN;
    if (color) {
        draw_sprite(ctx, &pg->mouse_sprite, pos_x, pos_y);
        if (pg->mouse_state == LEFT_CLICK) {
            fill_rect_solid(ctx, topleft, downright, *color);
        }
    } else {
        /* fill_rect_solid(ctx, topleft, downright, defalut_cursor_color); */
    }
}


static void draw_mouse_last_pos(gfx_context_t *ctx,
                                uint_32 mouse_x,
                                uint_32 mouse_y,
                                uint_32 last_mouse_x,
                                uint_32 last_mouse_y,
                                uint_32 bg)
{
    uint_32 width = 48;
    uint_32 height = 48;

    if (mouse_x > last_mouse_x && mouse_y > last_mouse_y) {
        // 1. Move from top left to down right
        /* printf("->"); */
        /* printf("v"); */
        uint_32 _h1_1 = mouse_y - last_mouse_y;
        uint_32 _w1_1 = width;
        uint_32 _x1_1 = last_mouse_x;
        uint_32 _y1_1 = last_mouse_y;

        uint_32 _h1_2 = height - _h1_1;
        uint_32 _w1_2 = mouse_x - last_mouse_x;
        uint_32 _x1_2 = last_mouse_x;
        uint_32 _y1_2 = last_mouse_y + _h1_1;

        rect_t rect1_1 = {
            .x = _x1_1, .y = _y1_1, .width = _w1_1, .height = _h1_1};
        rect_t rect1_2 = {
            .x = _x1_2, .y = _y1_2, .width = _w1_2, .height = _h1_2};

        gfx_add_clip(ctx, _x1_1, _y1_1, _w1_1, _h1_1);
        gfx_add_clip(ctx, _x1_2, _y1_2, _w1_2, _h1_2);
        draw_rect_solid(ctx, rect1_1, bg);
        draw_rect_solid(ctx, rect1_2, bg);

    } else if (mouse_x < last_mouse_x && mouse_y < last_mouse_y) {
        /* TWO_POINTS_TO_RECT(); */
        // 2. Move from down right to top left
        /* printf("<-"); x*/
        /* printf("^");  y*/
        point_t last_br_point = {.X = last_mouse_x + width,
                                 .Y = last_mouse_y + height};

        point_t curr_br_point = {.X = mouse_x + width, .Y = mouse_y + height};
        point_t last_bt_point = {.X = curr_br_point.X, .Y = last_br_point.Y};

        point_t curr_md_point = {.X = curr_br_point.X, .Y = last_mouse_y};
        point_t curr_bt_point = {.X = last_mouse_x, .Y = curr_br_point.Y};

        uint_32 _x_1 = curr_md_point.X;
        uint_32 _y_1 = curr_md_point.Y;

        rect_t rect2_1 =
            TWO_POINTS_TO_RECT(_x_1, _y_1, curr_md_point, last_br_point);

        gfx_add_clip(ctx, _x_1, _y_1, width, height);
        draw_rect_solid(ctx, rect2_1, bg);

        uint_32 _x_2 = curr_bt_point.X;
        uint_32 _y_2 = curr_bt_point.Y;
        rect_t rect2_2 =
            TWO_POINTS_TO_RECT(_x_2, _y_2, curr_bt_point, last_bt_point);

        gfx_add_clip(ctx, _x_2, _y_2, 0,
                     ABS(curr_bt_point.Y - last_bt_point.Y));
        draw_rect_solid(ctx, rect2_2, bg);

    } else if (mouse_x < last_mouse_x && mouse_y > last_mouse_y) {
        /* printf("<-"); x*/
        /* printf("v");  y*/
    } else if (mouse_x > last_mouse_x && mouse_y < last_mouse_y) {
        printf("->");  // x
        printf("^");   // y
        point_t last_br_point = {.X = last_mouse_x + width,
                                 .Y = last_mouse_y + height};
        point_t curr_br_point = {.X = mouse_x + width, .Y = mouse_y + height};
        point_t curr_bl_point = {.X = mouse_x, .Y = curr_br_point.Y};
        point_t last_bt_point = {.X = curr_br_point.X, .Y = last_br_point.Y};
        point_t last_ul_point = {.X = last_mouse_x, .Y = last_mouse_y};

        uint_32 _x_1 = last_mouse_x;
        uint_32 _y_1 = last_mouse_y;

        rect_t rect2_1 =
            TWO_POINTS_TO_RECT(_x_1, _y_1, last_ul_point, last_bt_point);

        gfx_add_clip(ctx, _x_1, _y_1, 0, height);
        draw_rect_solid(ctx, rect2_1, bg);

        uint_32 _x_2 = curr_bl_point.X;
        uint_32 _y_2 = curr_bl_point.Y;

        rect_t rect2_2 =
            TWO_POINTS_TO_RECT(_x_1, _y_1, curr_bl_point, last_br_point);

        gfx_add_clip(ctx, _x_2, _y_2, 0, rect2_2.height);
        draw_rect_solid(ctx, rect2_1, bg);
    }
}


/**
 * Blit a window to the framebuffer.
 *
 * TODO:
 * Applies transformations (rotation, animations) and then renders
 * the window through alpha blitting.
 */
static int poudland_blit_window(poudland_globals_t *pg,
                                poudland_server_window_t *window,
                                int x,
                                int y)
{
    if (window->hidden || window->minimized) {
        return 0;
    }

    sprite_t _win_sprite;
    _win_sprite.width = window->width;
    _win_sprite.height = window->height;
    _win_sprite.bitmap = (uint_32 *) window->buffer;
    _win_sprite.masks = NULL;
    _win_sprite.blank = 0;
    /* _win_sprite.alpha = ALPHA_EMBEDDED; */
    _win_sprite.alpha = ALPHA_OPAQUE;

    double opacity = (double) (window->opacity) / 255.0;

    if (window->opacity != 255) {
        // TODO:
        // Not support alpha yet
        draw_sprite_alpha(pg->backend_ctx, &_win_sprite, window->x, window->y,
                          opacity);
    } else {
        draw_sprite(pg->backend_ctx, &_win_sprite, window->x, window->y);
    }

    return 0;
}

static void poudland_blit_windows(poudland_globals_t *pg)
{
    if (!pg->bottom_z /*|| pg->bottom_z->anim_mode*/) {
        draw_fill(pg->backend_ctx, FSK_DARK_CYAN);
    }

    if (pg->bottom_z) {
        poudland_blit_window(pg, pg->bottom_z, pg->bottom_z->x,
                             pg->bottom_z->y);
    }

    struct list_head *node;
    list_for_each (node, &pg->mid_zs) {
        poudland_server_window_t *w =
            list_entry(node, poudland_server_window_t, server_w_mid_target);
        if (w)
            poudland_blit_window(pg, w, w->x, w->y);
    }

    if (pg->top_z)
        poudland_blit_window(pg, pg->top_z, pg->top_z->x, pg->top_z->y);
}


/**
 * Mark a given region as damaged.
 */
static void mark_region(poudland_globals_t *pg,
                        int_32 x,
                        int_32 y,
                        int_32 width,
                        int_32 height)
{
    damage_rect_t *rect = malloc(sizeof(damage_rect_t));

    rect->x = x;
    rect->y = y;
    rect->width = width;
    rect->height = height;

    list_add_tail(&rect->damage_region_target, &pg->update_list);
}

/**
 * Mark a region within a window as damaged.
 *
 * TODO:
 * If the window is rotated, we calculate the minimum rectangle that covers
 * the whole region specified and then mark that.
 */
static void mark_window_relative(poudland_globals_t *pg,
                                 poudland_server_window_t *window,
                                 int_32 x,
                                 int_32 y,
                                 int_32 width,
                                 int_32 height)
{
    if (window->hidden || window->minimized)
        return;
    damage_rect_t *rect = malloc(sizeof(damage_rect_t));
    poudland_server_window_t fake_window;

    if (window == pg->resizing_window) {
        fake_window.width = pg->resizing_init_w;
        fake_window.height = pg->resizing_init_h;
        fake_window.x = window->x;
        fake_window.y = window->y;
        fake_window.rotation = window->rotation;

        double x_scale =
            (double) pg->resizing_w / (double) pg->resizing_window->width;
        double y_scale =
            (double) pg->resizing_h / (double) pg->resizing_window->height;

        x *= x_scale;
        x += pg->resizing_offset_x - 1;

        y *= y_scale;
        y += pg->resizing_offset_y - 1;

        width *= x_scale;
        height *= y_scale;

        width += 2;
        height += 2;

        window = &fake_window;
    }

    if (window->rotation == 0) {
        rect->x = window->x + x;
        rect->y = window->y + y;
        rect->width = width;
        rect->height = height;
    } else {
        /* int_32 ul_x, ul_y; */
        /* int_32 ll_x, ll_y; */
        /* int_32 ur_x, ur_y; */
        /* int_32 lr_x, lr_y; */
        /*  */
        /* yutani_window_to_device(window, x, y, &ul_x, &ul_y); */
        /* yutani_window_to_device(window, x, y + height, &ll_x, &ll_y); */
        /* yutani_window_to_device(window, x + width, y, &ur_x, &ur_y); */
        /* yutani_window_to_device(window, x + width, y + height, &lr_x, &lr_y);
         */
        /*  */
        /* #<{(| Calculate bounds |)}># */
        /*  */
        /* int_32 left_bound = min(min(ul_x, ll_x), min(ur_x, lr_x)); */
        /* int_32 top_bound = min(min(ul_y, ll_y), min(ur_y, lr_y)); */
        /*  */
        /* int_32 right_bound = */
        /*     max(max(ul_x + 1, ll_x + 1), max(ur_x + 1, lr_x + 1)); */
        /* int_32 bottom_bound = */
        /*     max(max(ul_y + 1, ll_y + 1), max(ur_y + 1, lr_y + 1)); */
        /*  */
        /* rect->x = left_bound; */
        /* rect->y = top_bound; */
        /* rect->width = right_bound - left_bound; */
        /* rect->height = bottom_bound - top_bound; */
    }

    list_add_tail(&rect->damage_region_target, &pg->update_list);
}

/**
 * (Convenience function) Mark a whole a window as damaged.
 */
static void mark_window(poudland_globals_t *pg,
                        poudland_server_window_t *window)
{
    mark_window_relative(pg, window, 0, 0, window->width, window->height);
}

static void redraw_windows(struct poudland_globals *pg)
{
    argb_t col = FSK_RED;
    uint_8 need_update = 1;

    int_32 tmp_mouse_x = pg->mouse_x;
    int_32 tmp_mouse_y = pg->mouse_y;
    gfx_clear_clip(pg->backend_ctx);

    // Add damage region mouse damage region
    if (pg->last_mouse_x != tmp_mouse_x || pg->last_mouse_y != tmp_mouse_y ||
        pg->mouse_state == LEFT_CLICK) {
        need_update = 2;

        gfx_add_clip(pg->backend_ctx, pg->last_mouse_x, pg->last_mouse_y,
                     MOUSE_WIDTH, MOUSE_HEIGHT);

        gfx_add_clip(pg->backend_ctx, tmp_mouse_x, tmp_mouse_y, MOUSE_WIDTH,
                     MOUSE_HEIGHT);
    }
    pg->last_mouse_x = tmp_mouse_x;
    pg->last_mouse_y = tmp_mouse_y;

    /* Calculate damage regions from currently queued updates */
    while (!list_is_empty(&pg->update_list)) {
        struct list_head *target = list_pop(&pg->update_list);
        damage_rect_t *rect =
            list_entry(target, damage_rect_t, damage_region_target);

        /* We add a clip region for each window in the update queue */
        need_update = 1;
        gfx_add_clip(pg->backend_ctx, rect->x, rect->y, rect->width,
                     rect->height);
        free(rect);
    }

    /* Render */
    if (need_update) {
        // 1.Go through all windows and blit all windows to backer_buffer
        poudland_blit_windows(pg);

        // 2. draw cursor last.
        draw_mouse(pg, pg->mouse_x, pg->mouse_y, &col);
        flip(pg->backend_ctx);
    }
}

/**
 * test
 */
gfx_context_t *init_gfx_poudland_swindow(poudland_server_window_t *window)
{
    gfx_context_t *out = malloc(sizeof(gfx_context_t));
    out->width = window->width;
    out->height = window->height;
    out->stride = window->width * sizeof(uint_32);
    out->depth = 32;
    out->size = GFX_H(out) * GFX_W(out) * GFX_D(out);
    out->buffer = (char *) window->buffer;
    out->backbuffer = out->buffer;
    out->clips = NULL;
    return out;
}

gfx_context_t *init_graphics_poudland_swindows_double_buffer(
    poudland_server_window_t *window)
{
    gfx_context_t *out = init_gfx_poudland_swindow(window);
    out->backbuffer = malloc(GFX_D(out) * GFX_W(out) * GFX_H(out));
    return out;
}

poudland_server_window_t *quick_create_window(poudland_globals_t *global,
                                              uint_32 x,
                                              uint_32 y,
                                              uint_32 width,
                                              uint_32 height,
                                              uint_32 color)
{
    poudland_server_window_t *w =
        server_window_create(global, width, height, NULL, 0);
    w->hidden = 0;
    w->x = x;
    w->y = y;
    gfx_context_t *w_ctx = init_gfx_poudland_swindow(w);

    // fill w->buffer
    draw_fill(w_ctx, color);
    draw_pixel(w_ctx, 4, 4, FSK_CORN_SILK);
    rect_t _rect = {.x = 10, .y = 10, .width = 20, .height = 20};
    draw_rect_solid(w_ctx, _rect, FSK_RED);

    // mark window localtion at global->ctx
    mark_window(global, w);
    global->bottom_z = w;  // set bottom z axis windows
    return w;
}

static void window_move(poudland_globals_t *pg,
                        poudland_server_window_t *window,
                        int x,
                        int y)
{
    mark_window(pg, window);
    window->x = x;
    window->y = y;
    mark_window(pg, window);
}

int main(int argc, char *argv[])
{
    mouse_device_packet_t *mbuf = malloc(sizeof(mouse_device_packet_t));
    char *buf = malloc(1);
    uint_32 pkg_size = sizeof(mouse_device_packet_t);
    int_32 kbd_fd = open("/dev/input/event0", O_RDONLY);
    int_32 mouse_fd = open("/dev/input/event1", O_RDONLY);
    int_32 aux_fd = open("/dev/input/event2", O_RDONLY);
    int_32 tty_fd = open("/dev/tty0", O_RDWR);

    if (tty_fd == -1 || kbd_fd == -1 || mouse_fd == -1) {
        exit(0);
    }

    int_32 fds[2] = {kbd_fd, mouse_fd};
    struct timeval t2 = {.tv_sec = 0, .tv_usec = 20};

    poudland_globals_t *global = malloc(sizeof(poudland_globals_t));
    global->backend_ctx = init_gfx_fullscreen_double_buffer();
    ioctl(tty_fd, IO_CONSOLE_SET, &global->backend_ctx);

    // draw background
    uint_32 status_bar_color = 0x88131313;
    point_t top_left = {.X = 0, .Y = 0};
    point_t down_right = {.X = global->backend_ctx->width, .Y = 34};

    clear_screen(global->backend_ctx, FSK_DARK_BLUE);
    fill_rect_solid(global->backend_ctx, top_left, down_right,
                    status_bar_color);
    flip(global->backend_ctx);

    {
        struct timeval t;
        gettimeofday(&t, NULL);
        global->start_time = t.tv_sec;
        global->start_subtime = t.tv_usec;
    }

    global->width = global->backend_ctx->width;
    global->height = global->backend_ctx->height;

    global->backend_framebuffer = global->backend_ctx->backbuffer;

    /* inital poudland globals variables */
    global->last_mouse_x = 0;
    global->last_mouse_y = 0;
    global->mouse_x = global->width / 2;
    global->mouse_y = global->height / 2;

    INIT_LIST_HEAD(&global->windows);
    INIT_LIST_HEAD(&global->mid_zs);      /* regular windows list */
    INIT_LIST_HEAD(&global->update_list); /* damage rect list */
    INIT_LIST_HEAD(&global->menu_zs);     // not used yet
    INIT_LIST_HEAD(&global->overlay_zs);  // not used yet
                                          //
    // test code
    /* poudland_server_window_t *w1 = */
    /*     quick_create_window(global, 0, 0, global->width, global->height, */
    /*                         FSK_WHITE); */

    poudland_server_window_t *w1 =
        server_window_create(global, global->width, global->height, NULL, 0);
    w1->hidden = 0;
    w1->x = 0;
    w1->y = 0;
    gfx_context_t *w_ctx = init_gfx_poudland_swindow(w1);
    draw_fill(w_ctx, FSK_WHITE);
    draw_pixel(w_ctx, 4, 4, FSK_CORN_SILK);
    rect_t _rect = {.x = 10, .y = 10, .width = 20, .height = 20};
    draw_rect_solid(w_ctx, _rect, FSK_RED);
    mark_window(global, w1);
    ioctl(tty_fd, IO_CONSOLE_SET, &w_ctx);
    uint_32 set_color = FSK_CYAN;
    ioctl(tty_fd, IO_CONSOLE_COLOR, &set_color);
    printf("test");

    poudland_server_window_t *w2 =
        quick_create_window(global, 50, 50, 200, 200, FSK_DARK_OLIVE_GREEN);
    poudland_server_window_t *w3 =
        quick_create_window(global, 100, 100, 200, 200, FSK_DARK_CYAN);

    // load bmp image
    char *filename = "/b.bmp";
    int_32 img_fd = open(filename, O_RDONLY);
    /* int_32 image_fd = open("/b.jpg", O_RDONLY); */
    int meta[8] = {0};
    int a = bmp_meta(img_fd, meta);
    int imagesz = meta[1];
    int img_offset = meta[2];
    uint_32 *image = malloc(imagesz);
    int_32 width = meta[3];
    int_32 height = meta[4];

    lseek(img_fd, img_offset, SEEK_SET);
    read(img_fd, image, imagesz);

    global->mouse_sprite.bitmap = malloc(width * height * 4);
    global->mouse_sprite.width = width;
    global->mouse_sprite.height = height;
    global->mouse_sprite.alpha = ALPHA_OPAQUE;
    for (uint_32 h = 0; h < height; h++) {
        for (uint_32 w = 0; w < width; w++) {
            uint_32 color;
            color = image[w + width * h];
            /* draw_pixel(w3, w, h, color); */
            global->mouse_sprite.bitmap[w + width * h] = color;
        }
    }

    close(img_fd);
    free(image);

    // end test code

    uint_32 last_redraw = 0;
    while (1) {
        // this is a timer
        unsigned long frameTime = poudland_time_since(global, last_redraw);
        if (frameTime > 15) {
            redraw_windows(global);
            last_redraw = poudland_current_time(global);
            frameTime = 0;
        }

        int_32 idx = wait2(2, fds, &t2);
        if (idx == -1)
            continue;
        int_32 selected_fd = fds[idx];
        if (selected_fd == kbd_fd) {
            int step = 30;
            read(kbd_fd, buf, 1);
            // kbd_handler
            if (buf[0] == 'j') {
                int_32 y = w2->y;
                y += step;
                window_move(global, w2, w2->x, y);
            } else if (buf[0] == 'k') {
                int_32 y = w2->y;
                y -= step;
                window_move(global, w2, w2->x, y);
            } else if (buf[0] == 'h') {
                int_32 x = w2->x;
                x -= step;
                window_move(global, w2, x, w2->y);
            } else if (buf[0] == 'l') {
                int_32 x = w2->x;
                x += step;
                window_move(global, w2, x, w2->y);
            }
            /* printf("key event %c key press\n", buf[0]); */
        } else if (selected_fd == mouse_fd) {
            read(mouse_fd, mbuf, pkg_size);
            global->mouse_state = mbuf->buttons;
            global->mouse_x += mbuf->x_difference;
            global->mouse_y -= mbuf->y_difference;
            // mouse_handler
            if (global->mouse_x < 0) {
                global->mouse_x = 0;
            }
            if (global->mouse_y < 0) {
                global->mouse_y = 0;
            }
            point_t p = {.X = global->mouse_x, .Y = global->mouse_y};
            if (in_window(w2, p)) {
                if (mbuf->buttons == LEFT_CLICK) {
                    window_move(global, w2, global->mouse_x - 50,
                                global->mouse_y - 50);
                } else {
                    printf("(%d,%d)  ", p.X, p.Y);
                }
            }
        }
    }
}
