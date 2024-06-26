#include <gua/poudland.h>
#include <packetx.h>
#include <syscall.h>
#define COMPOSITOR_SERVER "compositor"

int main()
{
    poudland_t *ctx = poudland_init_context(COMPOSITOR_SERVER);
    int cfd = (int) ctx->sock;

    int win1 = poudland_create_window(ctx, cfd, FSK_ORANGE, 200, 200, 100, 100);
    int win2 = poudland_create_window(ctx, cfd, FSK_GREEN, 220, 200, 800, 400);
    int win3 = poudland_create_window(ctx, cfd, FSK_CYAN, 420, 200, 200, 800);

    poudland_remove_window(cfd, win3);
    uint_32 last_redraw = 0;
    bool is_removed = false;
    /* while (1) { */
    /*     unsigned long frameTime = poudland_time_since(global, last_redraw); */
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
