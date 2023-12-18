#include <gui/fsk_mouse.h>
#include <hid/mouse.h>
#include <ioqueue.h>
#include <ostype.h>

#include <fifo.h>
#include <string.h>
#include <sys/memory.h>
#include <sys/sched.h>
#include <sys/syscall.h>
#include <sys/threads.h>

// for test
#include <stdio.h>

extern CircleQueue mouse_queue;

static void read_packet_from_queue(CircleQueue *queue,
                                   mouse_device_packet_t *packet)
{
    uint_32 packet_size = sizeof(mouse_device_packet_t);
    char *cur = (char *) packet;
    while (packet_size) {
        char c = ioqueue_get_data(queue);
        *cur = c;
        cur++;
        packet_size--;
    }
}

static void mouse_event_handler(struct fsk_mouse *mouse)
{
    uint_32 testx = 0;
    uint_32 testy = 0;

    uint_32 cursor_x = mouse->point.X;
    uint_32 cursor_y = mouse->point.Y;
    gfx_context_t *ctx = mouse->ctx;
    free(mouse);

    FIFO *timer_queue = {0};
    char *buf = malloc(2);

    uint_32 org_color = fetch_color(ctx, cursor_x, cursor_y);
    uint_32 rcolor = FSK_LIME_GREEN;
    uint_32 lcolor = FSK_ORANGE_RED;
    uint_32 dccolor = FSK_GOLD;
    uint_32 *default_color = NULL;
    uint_8 double_left_click = 0;
    uint_32 double_click_delay;
    uint_8 hold_left_click = false;
    while (1) {
        mouse_device_packet_t packet = {0};
        read_packet_from_queue(&mouse_queue, &packet);

        if (packet.magic == MOUSE_MAGIC) {
            uint_32 delta_x = packet.x_difference;
            uint_32 delta_y = packet.y_difference;

            if ((cursor_x > 1 && cursor_x < ctx->width) &&
                (cursor_y > 0 && cursor_y < ctx->height)) {
                // redraw prev pixel
                draw_2d_gfx_cursor(ctx, cursor_x, cursor_y, &org_color);
            }

            // testcode
            uint_32 label_x, label_y= 0;
            char str[200] = {0};
            /* sprintf(str,"cursor(x:%d, y:%d)",cursor_x, cursor_y); */
            sprintf(str,"org color %x",org_color);
            uint_32 label_w = strlen(str) * 8 + 20;
            uint_32 label_h = 16;
            draw_2d_gfx_label(ctx, label_x, label_y, label_w, label_h,
                              FSK_ORANGE, FSK_MEDIUM_PURPLE, str);
            /*  */
            /* uint_32 label1_x = 0; */
            /* uint_32 label1_y = label_h + 20; */
            /* char str1[200] = {0}; */
            /* sprintf(str,"delta (x:%d, y:%d)",delta_x, delta_y); */
            /* uint_32 label1_w = strlen(str) * 8 + 20; */
            /* uint_32 label1_h = 16; */
            /* draw_2d_gfx_label(ctx, label1_x, label1_y, label1_w, label1_h, */
            /*                   FSK_ORANGE, FSK_MEDIUM_PURPLE, str); */
            // endtest

            bool is_cursorx_at_saftrange =
                (cursor_x + delta_x > 1 && cursor_x + delta_x < ctx->width);
            bool is_cursory_at_saftrange =
                (cursor_y + delta_y > 0 && cursor_y + delta_y < ctx->height);

            if (is_cursorx_at_saftrange && is_cursory_at_saftrange) {
                cursor_x += delta_x;
                cursor_y += (-delta_y);
                org_color = fetch_color(ctx, cursor_x, cursor_y);
                if (packet.buttons == LEFT_CLICK) {
                    if (double_left_click == 1 && fifo_rest(timer_queue) == 0) {
                        fifo_get_data(timer_queue);
                        double_left_click = 0;

                        // testcode
                        // Double left click event here
                        draw_2d_gfx_cursor(ctx, cursor_x, cursor_y, &dccolor);
                        draw_2d_gfx_asc_char(ctx, 8, cursor_x, cursor_y,
                                             dccolor, 0x03);
                        // testend
                    } else if (hold_left_click) {
                        // testcode
                        // hold left click event here
                        draw_2d_gfx_asc_char(ctx, 8, cursor_x, cursor_y,
                                             dccolor, 0x04);
                        // testend
                    } else {
                        double_left_click = 1;
                        uint_32 delay = 3500;  // 3500 comes up by test
                        init_fifo(timer_queue, 2, buf);
                        set_timer(delay, timer_queue, '1');
                        hold_left_click = true;

                        // testcode
                        // Single left click event here
                        draw_2d_gfx_cursor(ctx, cursor_x, cursor_y, &lcolor);
                        draw_2d_gfx_asc_char(ctx, 8, testx, 0, lcolor,
                                             packet.buttons);
                        testx += 16;
                        // testend
                    }
                } else if (packet.buttons == RIGHT_CLICK) {
                    double_left_click = 0;
                    hold_left_click = false;
                    // testcode
                    // mouse right single click event here
                    draw_2d_gfx_cursor(ctx, cursor_x, cursor_y, &rcolor);
                    // testend
                } else {  // moving with out button push
                    // testcode
                    // mouse move event here
                    draw_2d_gfx_cursor(ctx, cursor_x, cursor_y, default_color);
                    // testend
                    hold_left_click = false;
                }
            } else if (is_cursorx_at_saftrange && !is_cursory_at_saftrange) {
                draw_2d_gfx_cursor(ctx, cursor_x, cursor_y, default_color);
            } else if (is_cursory_at_saftrange && !is_cursorx_at_saftrange) {
                draw_2d_gfx_cursor(ctx, cursor_x, cursor_y, default_color);
            }
        }
    }
}

void create_fsk_mouse(gfx_context_t *ctx, uint_32 cursor_x, uint_32 cursor_y)
{
    struct fsk_mouse *mouse = malloc(sizeof(struct fsk_mouse));
    mouse->point.X = cursor_x;
    mouse->point.Y = cursor_y;
    mouse->ctx = ctx;
    thread_start("mouse_GUI", 40, mouse_event_handler, mouse);
}
