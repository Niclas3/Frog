// Window compositor

// watch mouse and keyboard event
#include <hid/mouse.h>
#include <ioqueue.h>
#include <ostype.h>
#include <poudland.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

static int_32 read_packet_from_queue(int_32 mouse_fd,
                                     mouse_device_packet_t *packet)
{
    uint_32 packet_size = sizeof(mouse_device_packet_t);
    char *cur = (char *) packet;
    while (packet_size) {
        char c[1] = {0};
        if (read(mouse_fd, c, 1) == 0) {
            memset(packet, 0, sizeof(mouse_device_packet_t));
            return 0;
        }
        *cur = c[0];
        cur++;
        packet_size--;
    }
    return 1;
}

static void check_keyboard(int_32 kbd_fd)
{
    uint_8 buf[1] = {0};
    int_32 len = read(kbd_fd, buf, 1);
    if (len > 0) {
        // send key message here
        printf("key: press %c", buf[0]);
    }
}

static void check_mouse(int_32 mouse_fd, mouse_device_packet_t *packet)
{
    int_32 res = read_packet_from_queue(mouse_fd, packet);
    if (res > 0) {
        printf("x:%d, y:%d\n", packet->x_difference, packet->y_difference);
    }
}

void poudland_main_loop(void)
{
    // keyboard event
    int_32 kbd_fd = open("/dev/input/event0", O_RDONLY);
    // mouse event
    int_32 mouse_fd = open("/dev/input/event1", O_RDONLY);
    mouse_device_packet_t packet = {0};
    while (1) {
        check_keyboard(kbd_fd);
        check_mouse(mouse_fd, &packet);
    }
    close(kbd_fd);
    close(mouse_fd);
}
