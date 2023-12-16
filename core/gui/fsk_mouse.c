#include <gui/fsk_mouse.h>
#include <hid/mouse.h>
#include <ioqueue.h>
#include <ostype.h>
#include <sys/2d_graphics.h>

#include <fifo.h>
#include <string.h>
#include <sys/memory.h>
#include <sys/sched.h>
#include <sys/threads.h>

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

static void mouse_event_handler(Point *pot)
{
    uint_32 testx = 0;
    uint_32 testy = 0;

    uint_32 cursor_x = pot->X;
    uint_32 cursor_y = pot->Y;
    sys_free(pot);

    FIFO *timer_queue = {0};
    char *buf = sys_malloc(2);

    uint_32 org_color = fetch_color(cursor_x, cursor_y);
    uint_32 rcolor = convert_argb(FSK_LIME_GREEN);
    uint_32 lcolor = convert_argb(FSK_ORANGE_RED);
    uint_32 dccolor = convert_argb(FSK_GOLD);
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

            // redraw prev pixel
            draw_2d_gfx_cursor(cursor_x, cursor_y, &org_color);

            // erase cursor
            cursor_x += delta_x;
            cursor_y += (-delta_y);

            org_color = fetch_color(cursor_x, cursor_y);

            if ((cursor_x > 1 && cursor_x < g_gfx_mode->x_resolution) &&
                (cursor_y > 0 && cursor_y < g_gfx_mode->y_resolution)) {
                if (packet.buttons == LEFT_CLICK) {
                    if (double_left_click == 1 && fifo_rest(timer_queue) == 0) {
                        fifo_get_data(timer_queue);
                        double_left_click = 0;

                        // testcode
                        // Double left click event here
                        draw_2d_gfx_cursor(cursor_x, cursor_y, &dccolor);
                        draw_2d_gfx_asc_char(8, cursor_x, cursor_y, dccolor,
                                             0x03);
                        // testend
                    } else if (hold_left_click) {
                        // testcode
                        // hold left click event here
                        draw_2d_gfx_asc_char(8, cursor_x, cursor_y, dccolor,
                                             0x04);
                        // testend
                    } else {
                        double_left_click = 1;
                        uint_32 delay = 3500;
                        init_fifo(timer_queue, 2, buf);
                        set_timer(delay, timer_queue, '1');
                        hold_left_click = true;

                        // testcode
                        // Single left click event here
                        draw_2d_gfx_cursor(cursor_x, cursor_y, &lcolor);
                        draw_2d_gfx_asc_char(8, testx, 0, lcolor,
                                             packet.buttons);
                        testx += 16;
                        // testend
                    }
                } else if (packet.buttons == RIGHT_CLICK) {
                    double_left_click = 0;
                    hold_left_click = false;
                    // testcode
                    // mouse right single click event here
                    draw_2d_gfx_cursor(cursor_x, cursor_y, &rcolor);
                    // testend
                } else {  // moving with out button push
                    // testcode
                    // mouse move event here
                    draw_2d_gfx_cursor(cursor_x, cursor_y, default_color);
                    // testend
                    hold_left_click = false;
                }
            }
        }
    }
}

void create_fsk_mouse(uint_32 cursor_x, uint_32 cursor_y)
{
    Point *base_point = sys_malloc(sizeof(base_point));
    base_point->X = cursor_x;
    base_point->Y = cursor_y;
    thread_start("mouse_GUI", 40, mouse_event_handler, base_point);
}
