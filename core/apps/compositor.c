#include <device/console.h>  // for ioctl(port_num)
#include <gua/2d_graphics.h>
#include <gua/poudland-server.h>
#include <hashmap.h>
#include <hid/mouse.h>
#include <list.h>
#include <math.h>
#include <packetx.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

#include <device/console.h>
#include "bmp.h"

static void mark_window(poudland_globals_t *pg,
                        poudland_server_window_t *window);

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

static bool in_window(rect_t *w, point_t *point)
{
    int x_max = w->x + w->width;
    int y_max = w->y + w->height;

    if ((point->X > x_max || point->X < w->x) ||
        (point->Y > y_max || point->Y < w->y)) {
        return false;
    } else {
        return true;
    }
}

static poudland_wid_t next_wid()
{
    static poudland_wid_t win_id_count = 1;
    return win_id_count++;
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

    win->wid = next_wid();
    win->owner = owner;
    list_add_tail(&win->server_w_target, &pg->windows);
    // add to mid_zs
    list_add_tail(&win->server_w_mid_target, &pg->mid_zs);
    hashmap_set(pg->wids_to_windows, (void *) win->wid, win);
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


    return win;
}

void server_window_close(poudland_globals_t *global, poudland_wid_t wid)
{
    poudland_server_window_t *w =
        hashmap_get(global->wids_to_windows, (const void *) wid);
    if (w) {
        if (w == global->focused_window) {
            global->focused_window = NULL;
        }
        mark_window(global, w);
        list_del_init(&w->server_w_target);
        list_del_init(&w->server_w_mid_target);
        hashmap_remove(global->wids_to_windows, (void *) w->wid);
        free(w->buffer);
        free(w);
    }
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
        if (pg->mouse_state == POUDLAND_MOUSE_STATE_NORMAL ||
            pg->mouse_state == POUDLAND_MOUSE_STATE_MOVING ||
            pg->mouse_state == POUDLAND_MOUSE_STATE_DRAGGING) {
            draw_sprite(ctx, &pg->mouse_sprite, pos_x, pos_y);
        } else {
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
    if (pg->last_mouse_x != tmp_mouse_x || pg->last_mouse_y != tmp_mouse_y) {
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

    /* struct list_head *node; */
    /* list_for_each (node, &pg->windows_to_remove) { */
    /*     poudland_server_window_t *w = */
    /*         list_entry(node, poudland_server_window_t, remove_target); */
    /*     if (w) { */
    /*         server_window_remove(pg, w); */
    /*     } */
    /* } */
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
                                              uint_32 color,
                                              uint_32 *owner)
{
    poudland_server_window_t *w =
        server_window_create(global, width, height, owner, 0);
    w->hidden = 0;
    w->x = x;
    w->y = y;

    gfx_context_t *w_ctx = init_gfx_poudland_swindow(w);
    // fill window backgroud color to w->buffer
    draw_fill(w_ctx, color);
#if 0
    // draw something at this window
    draw_pixel(w_ctx, 4, 4, FSK_CORN_SILK);
    rect_t _rect = {.x = 10, .y = 10, .width = 20, .height = 20};
    draw_rect_solid(w_ctx, _rect, FSK_RED);
#endif

    // mark window localtion at global->ctx
    mark_window(global, w);
    global->bottom_z = w;  // set bottom z axis windows
    return w;
}

bool is_target_window_in_all_windows(struct list_head *cur, void *value)
{
    point_t *p = malloc(sizeof(point_t));
    memcpy(p, value, sizeof(point_t));

    poudland_server_window_t *w =
        container_of(cur, poudland_server_window_t, server_w_target);
    rect_t r = {.x = w->x, .y = w->y, .width = w->width, .height = w->height};

    if (in_window(&r, p)) {
        return true;
    }

    return false;
}

bool is_in_mid_sz(struct list_head *cur, void *value)
{
    point_t *p = malloc(sizeof(point_t));
    memcpy(p, value, sizeof(point_t));

    poudland_server_window_t *w =
        container_of(cur, poudland_server_window_t, server_w_mid_target);
    rect_t r = {.x = w->x, .y = w->y, .width = w->width, .height = w->height};

    if (in_window(&r, p)) {
        return true;
    }

    return false;
}

static poudland_server_window_t *window_top_of(poudland_globals_t *global,
                                               uint_32 x,
                                               uint_32 y)
{
    if (global->focused_window) {
        rect_t focused_win = {.x = global->focused_window->x,
                              .y = global->focused_window->y,
                              .width = global->focused_window->width,
                              .height = global->focused_window->height};
        if (in_window(&focused_win, &(point_t){.X = x, .Y = y})) {
            return global->focused_window;
        }
    }
    // First search mid_zs
    /* global->mid_zs; */

    // Second menu_zs
    /* global->menu_zs; */

    // Third overlay_zs
    /* global->overlay_zs; */

    // final
    point_t p = {.X = x, .Y = y};
    struct list_head *target =
        list_walkerv2_prev(&global->mid_zs, is_in_mid_sz, &p);
    poudland_server_window_t *win =
        container_of(target, poudland_server_window_t, server_w_mid_target);
    return win;
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

// set target window to top
static void set_focus_window(poudland_globals_t *pg,
                             poudland_server_window_t *win)
{
    /* FIXME: wid == 1 is background temp */
    if (win->wid <= 1)
        return;
    pg->focused_window = win;
    pg->top_z = win;
    list_del_init(&win->server_w_mid_target);
    list_add_tail(&win->server_w_mid_target, &pg->mid_zs);
    mark_window(pg, win);
}

static poudland_server_window_t *get_focus_window(poudland_globals_t *global)
{
    if (!global->focused_window) {
        return global->bottom_z;
    } else {
        return global->focused_window;
    }
}

static void keyboard_handler(poudland_globals_t *global,
                             mouse_device_packet_t *package)
{
    /* if (buf[0] == 'j') { */
    /*     int_32 y = w2->y; */
    /*     y += step; */
    /*     window_move(global, w2, w2->x, y); */
    /* } else if (buf[0] == 'k') { */
    /*     int_32 y = w2->y; */
    /*     y -= step; */
    /*     window_move(global, w2, w2->x, y); */
    /* } else if (buf[0] == 'h') { */
    /*     int_32 x = w2->x; */
    /*     x -= step; */
    /*     window_move(global, w2, x, w2->y); */
    /* } else if (buf[0] == 'l') { */
    /*     int_32 x = w2->x; */
    /*     x += step; */
    /*     window_move(global, w2, x, w2->y); */
    /* } */
    /* printf("key event %c key press\n", buf[0]); */
}

void send_mouse_event_massage(uint_32 fd,
                              poudland_server_window_t *w,
                              uint_32 x,
                              uint_32 y,
                              uint_32 mouse_events,
                              uint_32 mouse_button)
{
    poudland_msg_t *msg = malloc(sizeof(poudland_msg_t) +
                                 sizeof(struct poudland_msg_mouse_event));
    msg->type = PL_MSG_MOUSE_EVENT;
    msg->magic = PL_MSG__MAGIC;
    msg->size =
        sizeof(poudland_msg_t) + sizeof(struct poudland_msg_mouse_event);
    struct poudland_msg_mouse_event *body =
        (struct poudland_msg_mouse_event *) msg->body;

    body->x = x;
    body->y = y;
    body->button_down = mouse_button;
    body->mouse_event_type = mouse_events;
    body->wid = w->wid;

    pkx_send(fd, w->owner, msg->size, (char *) msg);
}

static void mouse_handler(poudland_globals_t *global,
                          mouse_device_packet_t *package)
{
    global->mouse_x += package->x_difference;
    global->mouse_y -= package->y_difference;
    if (global->mouse_x < -MOUSE_OFFSET_X) {
        global->mouse_x = -MOUSE_OFFSET_X;
    }
    if (global->mouse_y < -MOUSE_OFFSET_Y) {
        global->mouse_y = -MOUSE_OFFSET_Y;
    }

    int_32 real_mouse_x = global->mouse_x + MOUSE_OFFSET_X;
    int_32 real_mouse_y = global->mouse_y + MOUSE_OFFSET_Y;

    int_32 real_last_mouse_x = global->last_mouse_x + MOUSE_OFFSET_X;
    int_32 real_last_mouse_y = global->last_mouse_y + MOUSE_OFFSET_Y;


    bool is_move = !((real_mouse_x == real_last_mouse_x) &&
                     (real_mouse_y == real_last_mouse_y));

    static uint_32 win_x_offset, win_y_offset;
    switch (global->mouse_state) {
    case POUDLAND_MOUSE_STATE_NORMAL: {
        if (!package->buttons && is_move) {
            global->mouse_state = POUDLAND_MOUSE_STATE_MOVING;
        } else if (package->buttons & POUDLAND_MOUSE_LEFT_CLICK && is_move) {
            poudland_server_window_t *w =
                window_top_of(global, real_mouse_x, real_mouse_y);
            global->mouse_state = POUDLAND_MOUSE_STATE_DRAGGING;
            if (w) {
                set_focus_window(global, w);
                global->mouse_window = w;
                win_x_offset = real_mouse_x - w->x;
                win_y_offset = real_mouse_y - w->y;
            }
        } else {
            poudland_server_window_t *w =
                window_top_of(global, real_mouse_x, real_mouse_y);
            if (package->buttons & POUDLAND_MOUSE_LEFT_CLICK) {
                if (w) {
                    set_focus_window(global, w);
                }
            }
            send_mouse_event_massage(global->server_fd, w, real_mouse_x,
                                     real_mouse_y, POUDLAND_MOUSE_EVENT_CLICK,
                                     package->buttons);
        }

        break;
    }
    case POUDLAND_MOUSE_STATE_MOVING: {
        poudland_server_window_t *w =
            window_top_of(global, real_mouse_x, real_mouse_y);
        if (package->buttons) {
            if (package->buttons & POUDLAND_MOUSE_LEFT_CLICK && w) {
                set_focus_window(global, w);
            }
            global->mouse_state = POUDLAND_MOUSE_STATE_NORMAL;
            send_mouse_event_massage(global->server_fd, w, real_mouse_x,
                                     real_mouse_y, POUDLAND_MOUSE_EVENT_CLICK,
                                     package->buttons);
            break;
        }
        send_mouse_event_massage(global->server_fd, w, real_mouse_x,
                                 real_mouse_y, POUDLAND_MOUSE_EVENT_MOVE,
                                 package->buttons);
        break;
    }
    case POUDLAND_MOUSE_STATE_DRAGGING: {
        // if mouse was not holding left button, then mouse_state sets to NORMAL
        poudland_server_window_t *w =
            window_top_of(global, real_mouse_x, real_mouse_y);
        if (package->buttons & POUDLAND_MOUSE_LEFT_CLICK) {
            // FIXME: wid == 1 is background temp
            if (global->mouse_window->wid > 1) {
                window_move(global, global->mouse_window,
                            real_mouse_x - win_x_offset,
                            real_mouse_y - win_y_offset);
                send_mouse_event_massage(
                    global->server_fd, w, real_mouse_x - win_x_offset,
                    real_mouse_y - win_y_offset, POUDLAND_MOUSE_EVENT_DRAG,
                    package->buttons);
            }
        } else {
            global->mouse_state = POUDLAND_MOUSE_STATE_NORMAL;
            send_mouse_event_massage(global->server_fd, w, real_mouse_x,
                                     real_mouse_y, POUDLAND_MOUSE_EVENT_RAISE,
                                     package->buttons);
        }
        break;
    }
    default: {
        break;
    }
    }
}

int main(int argc, char *argv[])
{
    int_32 tty_fd = open("/dev/tty0", O_RDWR);

    poudland_globals_t *global = malloc(sizeof(poudland_globals_t));
    global->backend_ctx = init_gfx_fullscreen_double_buffer();
    char *server_name = "compositor";
    global->server_ident = malloc(strlen(server_name));
    memcpy(global->server_ident, server_name, strlen(server_name));

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
    global->mouse_x = 0;
    global->mouse_y = 0;

    global->wids_to_windows = hashmap_init(20);

    INIT_LIST_HEAD(&global->windows);
    INIT_LIST_HEAD(&global->mid_zs);      /* regular windows list */
    INIT_LIST_HEAD(&global->update_list); /* damage rect list */
    INIT_LIST_HEAD(&global->menu_zs);     // not used yet menu layers windows
    INIT_LIST_HEAD(&global->overlay_zs);  // not used yet
    INIT_LIST_HEAD(&global->clients);     // all clients connect to this server

    /* set tty to backend_ctx*/
    ioctl(tty_fd, IO_CONSOLE_SET, &global->backend_ctx);

    int_32 serverfd = pkx_bind(global->server_ident);
    if (serverfd == -1) {
        exit(0);
    }
    global->server_fd = serverfd;

    // this is test client thread
#define COMPOSITOR_SERVER "compositor"
    if (!fork()) {
        poudland_t *ctx = poudland_init_context(COMPOSITOR_SERVER);
        int cfd = (int) ctx->sock;

        int win1 =
            poudland_create_window(ctx, cfd, FSK_ORANGE, 200, 200, 100, 100);
        int win2 =
            poudland_create_window(ctx, cfd, FSK_GREEN, 220, 200, 800, 400);
        int win3 =
            poudland_create_window(ctx, cfd, FSK_DARK_RED, 420, 200, 200, 800);

        poudland_remove_window(cfd, win3);
        uint_32 last_redraw = 0;
        bool is_removed = false;
        /* while (1) */
        /*     ; */
        /* while (1) { */
        /*     unsigned long frameTime = poudland_time_since(global,
         * last_redraw); */
        /*     if (frameTime > 60000) { */
        /*         if (!is_removed) { */
        /*             poudland_remove_window(cfd, win3); */
        /*             is_removed = true; */
        /*         } */
        /*         last_redraw = poudland_current_time(global); */
        /*         frameTime = 0; */
        /*     } */
        /* } */


        while (1) {
            if (pkx_query(cfd)) {
                char *pkg = malloc(PACKET_SIZE);
                pkx_recv(cfd, pkg);
                poudland_msg_t *m = (poudland_msg_t *) pkg;
                switch (m->type) {
                case PL_MSG_MOUSE_EVENT: {
                    struct poudland_msg_mouse_event *msg =
                        (struct poudland_msg_mouse_event *) m->body;
                    /* if (msg->mouse_event_type == POUDLAND_MOUSE_EVENT_CLICK
                     * && */
                    /*     msg->wid > 1) { */
                    /*     if (!is_removed) { */
                    /*         poudland_remove_window(cfd, msg->wid); */
                    /*         is_removed = true; */
                    /*     } */
                    /* } */
                    if (msg->mouse_event_type == POUDLAND_MOUSE_EVENT_DRAG) {
                        /* poudland_move_window(cfd, msg->wid, msg->x, msg->y);
                         */
                    }
                    break;
                }
                case PL_MSG_WINDOW_MOVE: {
                    struct poudland_msg_win_move *msg =
                        (struct poudland_msg_win_move *) m->body;

                    break;
                }
                defalut : {
                    break;
                }
                }
                free(pkg);
            }
        }
    }
    // end test thread


    int_32 kbd_fd = open("/dev/input/event0", O_RDONLY);
    int_32 mouse_fd = open("/dev/input/event1", O_RDONLY);
    /* int_32 aux_fd = open("/dev/input/event2", O_RDONLY); */

    if (tty_fd == -1 || kbd_fd == -1 || mouse_fd == -1) {
        exit(0);
    }

    int_32 fds[2] = {kbd_fd, mouse_fd};
    struct timeval t2 = {.tv_sec = 0, .tv_usec = 20};

    // test code
    poudland_server_window_t *bg =
        server_window_create(global, global->width, global->height, NULL, 0);
    bg->hidden = 0;
    bg->x = 0;
    bg->y = 0;
    gfx_context_t *w_ctx = init_gfx_poudland_swindow(bg);
    draw_fill(w_ctx, FSK_WHITE);
    draw_pixel(w_ctx, 4, 4, FSK_CORN_SILK);
    rect_t _rect = {.x = 10, .y = 10, .width = 20, .height = 20};
    draw_rect_solid(w_ctx, _rect, FSK_RED);
    mark_window(global, bg);
    /* global->bottom_z = bg; */

    ioctl(tty_fd, IO_CONSOLE_SET, &w_ctx);
    uint_32 set_color = FSK_BLACK;
    ioctl(tty_fd, IO_CONSOLE_COLOR, &set_color);

    // load bmp image as cursor
    char *filename = "/b.bmp";
    int_32 img_fd = open(filename, O_RDONLY);
    int meta[8] = {0};
    int a = bmp_meta(img_fd, meta);
    int imagesz = meta[1];
    int img_offset = meta[2];
    uint_32 *image = malloc(imagesz);
    int_32 width = meta[3];
    int_32 height = meta[4];

    /* load_bmp_image(filename, image); */

    lseek(img_fd, img_offset, SEEK_SET);
    read(img_fd, image, imagesz);

    global->mouse_sprite.bitmap = malloc(width * height * 4);
    global->mouse_sprite.width = width;
    global->mouse_sprite.height = height;
    global->mouse_sprite.alpha = ALPHA_EMBEDDED;
    for (uint_32 h = 0; h < height; h++) {
        for (uint_32 w = 0; w < width; w++) {
            uint_32 color;
            color = image[w + width * h];
            global->mouse_sprite.bitmap[w + width * h] = color;
        }
    }

    close(img_fd);
    free(image);
    // end test code

    char *kbuf = malloc(1);
    mouse_device_packet_t *mbuf = malloc(sizeof(mouse_device_packet_t));

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
        if (idx == -1) {
            if (pkx_query(serverfd)) {
                packetx_t *packet = malloc(PACKET_SIZE);
                pkx_listen(serverfd, packet);
                poudland_msg_t *m = (poudland_msg_t *) packet->data;
                if (m->magic != PL_MSG__MAGIC) {
                    // some thing goes wrong.
                    free(packet);
                    exit(0);
                }
                switch (m->type) {
                case PL_MSG_HELLO: {
                    poudland_msg_t *msg =
                        malloc(sizeof(poudland_msg_t) +
                               (sizeof(struct poudland_hello_msg_t)));
                    msg->type = PL_MSG_WELCOME;
                    msg->size = sizeof(poudland_msg_t) +
                                sizeof(struct poudland_hello_msg_t);
                    struct poudland_hello_msg_t *hm =
                        (struct poudland_hello_msg_t *) &msg->body;
                    hm->width = global->width;
                    hm->height = global->height;
                    pkx_send(serverfd, (uint_32 *) packet->source, msg->size,
                             (char *) msg);
                    break;
                }
                case PL_MSG_WINDOW_NEW: {
                    // 1. get width and height from client
                    struct poudland_msg_window_new *msg =
                        (struct poudland_msg_window_new *) m->body;
                    struct poudland_server_window *w = quick_create_window(
                        global, msg->pos_x, msg->pos_y, msg->width, msg->height,
                        msg->color, packet->source);
                    poudland_msg_t *pm =
                        malloc(sizeof(poudland_msg_t) +
                               sizeof(struct poudland_msg_window_new_init));
                    pm->size = sizeof(poudland_msg_t) +
                               sizeof(struct poudland_msg_window_new_init);
                    pm->magic = PL_MSG__MAGIC;
                    pm->type = PL_MSG_WINDOW_INIT;
                    struct poudland_msg_window_new_init *m =
                        (struct poudland_msg_window_new_init *) pm->body;
                    m->wid = w->wid;
                    m->width = msg->width;
                    m->height = msg->height;
                    m->color = msg->color;
                    m->pos_x = msg->pos_x;
                    m->pos_y = msg->pos_y;
                    pkx_send(serverfd, (uint_32 *) packet->source, pm->size,
                             (char *) pm);
                    break;
                }
                case PL_MSG_WINDOW_MOVE: {
                    struct poudland_msg_win_move *msg =
                        (struct poudland_msg_win_move *) m->body;
                    if (msg->x > (int) global->width ||
                        msg->y > (int) global->height || msg->x < 0 ||
                        msg->y < 0) {
                        break;
                    }
                    poudland_server_window_t *w =
                        hashmap_get(global->wids_to_windows, msg->win_id);
                    if (w) {
                        window_move(global, w, msg->x, msg->y);
                    }
                    break;
                }
                case PL_MSG_WINDOW_MOUSE_EVENT: {
                    break;
                }
                case PL_MSG_WINDOW_CLOSE: {
                    struct poudland_msg_window_close *mm =
                        (struct poudland_msg_window_close *) m->body;
                    if (hashmap_has(global->wids_to_windows, mm->wid)) {
                        server_window_close(global, mm->wid);
                    }
                    break;
                }
                default:
                    break;
                }
                free(packet);
            }
            continue;
        }
        int_32 selected_fd = fds[idx];
        if (selected_fd == kbd_fd) {
            int step = 30;
            if (read(kbd_fd, kbuf, 1) > 0) {
                keyboard_handler(global, kbuf);
            }

        } else if (selected_fd == mouse_fd) {
            read(mouse_fd, mbuf, sizeof(mouse_device_packet_t));
            mouse_handler(global, mbuf);
        }
    }
}
