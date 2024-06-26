// Window compositor

// watch mouse and keyboard event
#include <gua/poudland.h>
#include <ostype.h>
#include <packetx.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>

// Waiting for specific type message
poudland_msg_t *poudland_wait_for(poudland_t *p, uint_32 type)
{
    while (1) {
        if (pkx_query((int_32) p->sock)) {
            char msg[MAX_PACKET_DATA_SIZE];
            int size = pkx_recv((int) p->sock, msg);
            poudland_msg_t *m = malloc(size);
            memcpy(m, msg, size);
            if (m->type == type) {
                return m;
            } else {
                // add message to queue
                list_add_tail(&p->equeue, &m->target);
            }
        }
    }
}

poudland_t *poudland_create_context(int_32 sock)
{
    poudland_t *ctx = malloc(sizeof(poudland_t));
    ctx->windows = hashmap_init(10);
    INIT_LIST_HEAD(&ctx->equeue);
    ctx->sock = (void *) (int_32) sock;
    return ctx;
}

poudland_t *poudland_init_context(const char *server)
{
    int_32 fd = pkx_connect((char *) server);
    if (fd == -1) {
        return NULL;
    }
    poudland_t *ctx = poudland_create_context(fd);
    ctx->server_ident = malloc(strlen(server));
    memcpy(ctx->server_ident, server, strlen(server));
    poudland_msg_t *msg =
        malloc(sizeof(poudland_msg_t) + sizeof(struct poudland_hello_msg_t));
    msg->type = PL_MSG_HELLO;
    msg->magic = PL_MSG__MAGIC;
    msg->size = sizeof(struct poudland_hello_msg_t);
    pkx_reply(fd, msg->size, (char *) msg);

    poudland_msg_t *mm = poudland_wait_for(ctx, PL_MSG_WELCOME);
    struct poudland_hello_msg_t *m = (struct poudland_hello_msg_t *) &mm->body;
    ctx->display_width = m->width;
    ctx->display_height = m->height;
    free(mm);
    return ctx;
}

/**
 * Create a graphical context around a poudland window.
 */
gfx_context_t *init_graphics_poudland(poudland_window_t *window)
{
    gfx_context_t *out = malloc(sizeof(gfx_context_t));
    out->width = window->width;
    out->height = window->height;
    out->stride = window->width * sizeof(uint_32);
    out->depth = 32;
    out->size = GFX_H(out) * GFX_W(out) * GFX_D(out);
    out->buffer = window->buffer;
    out->backbuffer = out->buffer;
    out->clips = NULL;
    return out;
}

gfx_context_t *init_graphics_poudland_double_buffer(poudland_window_t *window)
{
    gfx_context_t *out = init_graphics_poudland(window);
    out->backbuffer = malloc(GFX_D(out) * GFX_W(out) * GFX_H(out));
    return out;
}

void send_hello_message(int_32 sfd)
{
    poudland_msg_t *msg = malloc(sizeof(poudland_msg_t));
    msg->type = PL_MSG_HELLO;
    sprintf((char *) msg->body, "message from client");
    msg->size = sizeof(poudland_msg_t) + 20;
    msg->magic = PL_MSG__MAGIC;
    pkx_reply(sfd, msg->size, (char *) msg);
}

void send_move_window_massage(int_32 serverfd, int wid, int x, int y)
{
    int move_msg_size =
        sizeof(poudland_msg_t) + sizeof(struct poudland_msg_win_move);
    poudland_msg_t *mm = malloc(move_msg_size);
    mm->size = move_msg_size;
    mm->type = PL_MSG_WINDOW_MOVE;
    mm->magic = PL_MSG__MAGIC;
    struct poudland_msg_win_move *move_msg =
        (struct poudland_msg_win_move *) mm->body;
    move_msg->x = x;
    move_msg->y = y;
    move_msg->win_id = wid;
    pkx_reply(serverfd, mm->size, (char *) mm);
}

void poudland_move_window(int_32 serverfd, int wid, int x, int y)
{
    send_move_window_massage(serverfd, wid, x, y);
}

void send_create_window_massage(int_32 sfd,
                                int_32 color,
                                int_32 x,
                                int_32 y,
                                int_32 width,
                                int_32 height)
{
    uint_32 size_body = sizeof(struct poudland_msg_window_new);
    poudland_msg_t *msg = malloc(sizeof(poudland_msg_t) + size_body);
    msg->magic = PL_MSG__MAGIC;
    msg->type = PL_MSG_WINDOW_NEW;
    struct poudland_msg_window_new *m =
        (struct poudland_msg_window_new *) msg->body;
    m->width = width;
    m->height = height;
    m->pos_x = x;
    m->pos_y = y;
    m->color = color;
    msg->size = sizeof(poudland_msg_t) + size_body;
    pkx_reply(sfd, msg->size, (char *) msg);
}

uint_32 poudland_create_window(poudland_t *ctx,
                                      int_32 fd,
                                      int_32 color,
                                      int_32 x,
                                      int_32 y,
                                      int_32 width,
                                      int_32 height)
{
    send_create_window_massage(fd, color, x, y, width, height);
    poudland_msg_t *win_msg = poudland_wait_for(ctx, PL_MSG_WINDOW_INIT);
    struct poudland_msg_window_new_init *win_init_msg =
        (struct poudland_msg_window_new_init *) win_msg->body;
    free(win_msg);
    return win_init_msg->wid;
}

void poudland_remove_window(int_32 fd, int_32 wid)
{
    uint_32 size_body = sizeof(struct poudland_msg_window_close);
    poudland_msg_t *msg = malloc(sizeof(poudland_msg_t) + size_body);
    msg->magic = PL_MSG__MAGIC;
    msg->type = PL_MSG_WINDOW_CLOSE;
    msg->size = sizeof(poudland_msg_t) + size_body;
    struct poudland_msg_window_close *m =
        (struct poudland_msg_window_close *) msg->body;
    m->wid = wid;
    pkx_reply(fd, msg->size, (char *) msg);
}
