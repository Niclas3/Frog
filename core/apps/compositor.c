#include <sys/syscall.h>
#include <hid/mouse.h>
#include <stdio.h>
#include <gua/2d_graphics.h>
#include <device/console.h> // for ioctl(port_num)
#include <gua/poudland-server.h>

#include <device/console.h>

void redraw_windows(struct poudland_globals *p_glb);

int main(int argc, char * argv[]) {
    mouse_device_packet_t *mbuf = malloc(sizeof(mouse_device_packet_t));
    char *buf = malloc(1);
    uint_32 pkg_size = sizeof(mouse_device_packet_t);
    int_32 kbd_fd = open("/dev/input/event0", O_RDONLY);
    int_32 mouse_fd = open("/dev/input/event1", O_RDONLY);
    int_32 aux_fd = open("/dev/input/event2", O_RDONLY);

    int_32 tty_fd = open("/dev/tty0", O_RDWR);
    if(tty_fd == -1 || kbd_fd == -1 || mouse_fd == -1){
        exit(0);
    }

    int_32 fds[2] = {kbd_fd, mouse_fd};
    struct timeval t2 = {.tv_sec = 16, .tv_usec = 0};

    poudland_globals_t *global = malloc(sizeof(poudland_globals_t));
    global->backend_ctx = init_gfx_fullscreen_double_buffer();
    ioctl(tty_fd, IO_CONSOLE_SET, &global->backend_ctx);

    global->mouse_x = 200;
    global->mouse_y = 200;

    //draw background
    uint_32 status_bar_color = 0x88131313;
    Point top_left = {.X = 0, .Y = 0};
    Point down_right = {.X = global->backend_ctx->width, .Y = 34};

    clear_screen(global->backend_ctx, FSK_DARK_BLUE);
    fill_rect_solid(global->backend_ctx, top_left, down_right, status_bar_color);
    flip(global->backend_ctx);

    while (1) {
        int_32 idx = wait2(2, fds, &t2);
        if (idx == -1) {
            // no interrupt happends
            redraw_windows(global);  // _redraw() every 16s
            continue;
        }
        int_32 selected_fd = fds[idx];
        if (selected_fd == kbd_fd) {
            printf("No.%d fd is wake \n", idx);
            read(kbd_fd, buf, 1);
            printf("key event %c key press\n", buf[0]);
        } else if (selected_fd == mouse_fd) {
            /* printf("No.%d fd is wake \n", idx); */
            read(mouse_fd, mbuf, pkg_size);
            struct timeval t = {0};
            gettimeofday(&t, NULL);
            /* printf("%d: %d   ", t.tv_sec, t.tv_usec); */
            /* printf("mouse event:(x:%d, y:%d)\n", mbuf->x_difference, */
            /*        mbuf->y_difference); */
            global->last_mouse_x = global->mouse_x;
            global->last_mouse_y = global->mouse_y;

            global->mouse_x += mbuf->x_difference * 3;
            global->mouse_y -= mbuf->y_difference * 3;
        }
        redraw_windows(global);  // _redraw() every 16s
    }
}

void redraw_windows(struct poudland_globals *p_glb)
{
    argb_t col = FSK_GOLD;
    argb_t bg = FSK_DARK_BLUE;
    gfx_context_t *ctx = p_glb->backend_ctx;

    // mouse draw test
    gfx_add_clip(p_glb->backend_ctx, p_glb->mouse_x, p_glb->mouse_y, 4, 4);
    draw_2d_gfx_cursor(ctx, p_glb->mouse_x, p_glb->mouse_y, &col);
    gfx_add_clip(p_glb->backend_ctx, p_glb->last_mouse_x, p_glb->last_mouse_y, 4, 4);
    draw_2d_gfx_cursor(ctx, p_glb->last_mouse_x, p_glb->last_mouse_y, &bg);
    flip(ctx);
    gfx_clear_clip(p_glb->backend_ctx);
}


