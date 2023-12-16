#include <gui/fsk_mouse.h>
#include <hid/mouse.h>
#include <ioqueue.h>
#include <ostype.h>
#include <sys/2d_graphics.h>

#include <sys/memory.h>
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
                    // Double click left
                    if (double_left_click == 1 && double_click_delay > 0) {
                        double_left_click = 0;
                        draw_2d_gfx_cursor(cursor_x, cursor_y, &dccolor);
                        // testcode
                        draw_2d_gfx_asc_char(8, cursor_x, cursor_y, dccolor,
                                             0x03);
                        // testend
                    } else if (hold_left_click) {
                        // testcode
                        draw_2d_gfx_asc_char(8, cursor_x, cursor_y, dccolor,
                                             0x04);
                        // testend
                    } else {
                        draw_2d_gfx_cursor(cursor_x, cursor_y, &lcolor);
                        double_left_click = 1;
                        double_click_delay = 3;
                        hold_left_click = true;

                        // testcode
                        draw_2d_gfx_asc_char(8, testx, 0, lcolor,
                                             packet.buttons);
                        testx += 16;
                        // testend
                    }
                } else if (packet.buttons == RIGHT_CLICK) {
                    draw_2d_gfx_cursor(cursor_x, cursor_y, &rcolor);
                    double_left_click = 0;
                    hold_left_click = false;
                } else {  // moving with out button push
                    draw_2d_gfx_cursor(cursor_x, cursor_y, default_color);
                    hold_left_click = false;
                }
            }
        }
        // count down double_click_delay
        // TODO:
        // replace it to CMOS time
        if (double_click_delay > 0) {
            double_click_delay--;
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
