#include <sys/syscall.h>
#include <hid/mouse.h>
#include <stdio.h>
#include <gua/2d_graphics.h>
#include <gua/poudland-server.h>

static void redraw_windows(struct poudland_globals *p_glb)
{
    gfx_context_t *ctx = p_glb->backend_ctx;
    flip(ctx);
}

int main(int argc, char * argv[]) {
    mouse_device_packet_t *mbuf = malloc(sizeof(mouse_device_packet_t));
    char *buf = malloc(1);
    uint_32 pkg_size = sizeof(mouse_device_packet_t);
    int_32 kbd_fd = open("/dev/input/event0", O_RDONLY);
    int_32 mouse_fd = open("/dev/input/event1", O_RDONLY);
    int_32 aux_fd = open("/dev/input/event2", O_RDONLY);

    int_32 fds[2] = {kbd_fd, mouse_fd};
    struct timeval t2 = {.tv_sec = 16, .tv_usec = 0};

    while (1) {
        int_32 idx = wait2(2, fds, &t2);
        if (idx == 0) {
            printf("No.%d fd is wake \n", idx);
            read(kbd_fd, buf, 1);
            printf("key event %c key press\n", buf[0]);
        } else if (idx == 1) {
            printf("No.%d fd is wake \n", idx);
            read(mouse_fd, mbuf, pkg_size);
            printf("mouse event:(x:%d, y:%d)\n", mbuf->x_difference,
                   mbuf->y_difference);
        } else if (idx == 2) {
            printf("No.%d fd is wake \n", idx);
            read(aux_fd, buf, 1);
            printf("aux data %x \n", buf[0]);
        } else if (idx == -1) {
            printf("timeout is here\n");
        }
    }
}
