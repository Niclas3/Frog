#ifndef POUDLAND_P0_SERVER_H
#define POUDLAND_P0_SERVER_H

#include "protocol.h"

#include <frog/packagefs.h>

#define POUDLAND_P0_REPLY_QUEUE_MAX 8U

struct poudland_p0_scene;
struct poudland_p0_server;

typedef int_32 (*poudland_p0_server_send_fn)(
    struct poudland_p0_server *server, frog_pkg_peer_id peer_id,
    const void *payload, uint_32 payload_size);
typedef void (*poudland_p0_server_damage_fn)(
    struct poudland_p0_server *server, struct poudland_p0_rect rect);

struct poudland_p0_server_reply {
        uint_32 size;
        uint_8 data[POUDLAND_V1_P0_MESSAGE_MAX];
};

struct poudland_p0_server_peer {
        bool active;
        bool tombstone;
        bool closing;
        frog_pkg_peer_id peer_id;
        uint_32 reply_head;
        uint_32 reply_count;
        struct poudland_p0_server_reply
            replies[POUDLAND_P0_REPLY_QUEUE_MAX];
};

struct poudland_p0_server {
        int_32 fd;
        struct poudland_p0_protocol protocol;
        struct poudland_p0_server_peer
            peers[POUDLAND_P0_PROTOCOL_SESSION_MAX];
        poudland_p0_server_send_fn send;
        poudland_p0_server_damage_fn damage;
        void *transport_context;
};

void poudland_p0_server_state_init(
    struct poudland_p0_server *server, uint_32 display_width,
    uint_32 display_height, poudland_p0_server_send_fn send,
    poudland_p0_server_damage_fn damage, void *transport_context);
bool poudland_p0_server_handle_record(
    struct poudland_p0_server *server,
    const struct frog_pkg_message *message);

#ifndef POUDLAND_P0_SERVER_HOST_TEST
int_32 poudland_p0_server_open(struct poudland_p0_server *server,
                               struct poudland_p0_scene *scene);
bool poudland_p0_server_run(struct poudland_p0_server *server,
                            struct poudland_p0_scene *scene);
void poudland_p0_server_close(struct poudland_p0_server *server);
#endif

#endif
