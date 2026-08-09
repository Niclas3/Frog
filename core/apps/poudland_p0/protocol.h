#ifndef POUDLAND_P0_PROTOCOL_H
#define POUDLAND_P0_PROTOCOL_H

#include "geometry.h"

#include <gua/poudland_protocol.h>

#define POUDLAND_P0_PROTOCOL_SESSION_MAX 16U
#define POUDLAND_P0_PROTOCOL_DAMAGE_MAX  POUDLAND_V1_SESSION_WINDOW_MAX

struct poudland_p0_protocol_window {
        bool active;
        uint_32 id;
        uint_32 owner_peer_id;
        struct poudland_p0_rect bounds;
        uint_32 color;
        uint_8 last_key;
};

struct poudland_p0_protocol_session {
        bool active;
        bool welcomed;
        uint_32 peer_id;
        uint_32 window_count;
};

struct poudland_p0_protocol {
        struct poudland_p0_protocol_session
            sessions[POUDLAND_P0_PROTOCOL_SESSION_MAX];
        struct poudland_p0_protocol_window
            windows[POUDLAND_V1_SERVER_WINDOW_MAX];
        uint_32 display_width;
        uint_32 display_height;
        uint_32 next_window_id;
        uint_32 window_count;
};

struct poudland_p0_protocol_result {
        uint_32 reply_size;
        uint_32 damage_count;
        uint_8 reply[POUDLAND_V1_P0_MESSAGE_MAX];
        bool terminate_session;
        struct poudland_p0_rect damages[POUDLAND_P0_PROTOCOL_DAMAGE_MAX];
};

typedef char poudland_p0_protocol_reply_must_be_u32_aligned[
    (__builtin_offsetof(struct poudland_p0_protocol_result, reply) & 3U) == 0
        ? 1
        : -1];

void poudland_p0_protocol_init(struct poudland_p0_protocol *protocol,
                               uint_32 display_width,
                               uint_32 display_height);
int_32 poudland_p0_protocol_handle_data(
    struct poudland_p0_protocol *protocol, uint_32 peer_id,
    const void *message, uint_32 message_size,
    struct poudland_p0_protocol_result *result);
void poudland_p0_protocol_handle_disconnect(
    struct poudland_p0_protocol *protocol, uint_32 peer_id,
    struct poudland_p0_protocol_result *result);

#endif
