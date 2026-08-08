#ifndef _GUA_POUDLAND_V1_H
#define _GUA_POUDLAND_V1_H

#include <frog/types.h>
#include <gua/poudland_protocol.h>

#define POUDLAND_V1_PENDING_MAX       16U
#define POUDLAND_V1_INBOX_CAPACITY    8U
#define POUDLAND_V1_EVENT_CAPACITY    8U
#define POUDLAND_V1_QUEUED_PAYLOAD_MAX 32U
#define POUDLAND_V1_CONTEXT_SIZE_MAX  2048U

struct poudland_v1_message {
        struct poudland_v1_header header;
        uint_8 payload[POUDLAND_V1_P0_PAYLOAD_MAX];
};

struct poudland_v1_pending {
        uint_32 request_id;
        uint_32 request_type;
        uint_32 response_type;
        uint_32 occupied;
};

struct poudland_v1_queued_message {
        struct poudland_v1_header header;
        uint_8 payload[POUDLAND_V1_QUEUED_PAYLOAD_MAX];
};

struct poudland_v1_context {
        int_32 fd;
        uint_16 version;
        uint_16 connected;
        uint_32 display_width;
        uint_32 display_height;
        uint_32 client_capabilities;
        uint_32 server_capabilities;
        uint_32 next_request_id;
        struct poudland_v1_pending pending[POUDLAND_V1_PENDING_MAX];
        struct poudland_v1_queued_message inbox[POUDLAND_V1_INBOX_CAPACITY];
        struct poudland_v1_queued_message events[POUDLAND_V1_EVENT_CAPACITY];
        uint_32 inbox_count;
        uint_32 event_head;
        uint_32 event_count;
};

typedef char poudland_v1_context_must_be_bounded[
    sizeof(struct poudland_v1_context) <= POUDLAND_V1_CONTEXT_SIZE_MAX ? 1 : -1];
typedef char poudland_v1_queued_payload_must_cover_p0[
    sizeof(struct poudland_v1_welcome) <= POUDLAND_V1_QUEUED_PAYLOAD_MAX &&
            sizeof(struct poudland_v1_window_init) <=
                POUDLAND_V1_QUEUED_PAYLOAD_MAX &&
            sizeof(struct poudland_v1_window_closed) <=
                POUDLAND_V1_QUEUED_PAYLOAD_MAX &&
            sizeof(struct poudland_v1_error) <=
                POUDLAND_V1_QUEUED_PAYLOAD_MAX &&
            sizeof(struct poudland_v1_window_configure) <=
                POUDLAND_V1_QUEUED_PAYLOAD_MAX &&
            sizeof(struct poudland_v1_pointer_event) <=
                POUDLAND_V1_QUEUED_PAYLOAD_MAX &&
            sizeof(struct poudland_v1_key_event) <=
                POUDLAND_V1_QUEUED_PAYLOAD_MAX
        ? 1
        : -1];

void poudland_v1_context_init(struct poudland_v1_context *context);

/* Low-level requests return zero after one complete packagefs send. A send
 * failure occupies no pending slot. Only a consumed response or ERROR frees
 * the pending request; -ETIMEDOUT deliberately leaves it waitable. */
int_32 poudland_v1_begin_request(struct poudland_v1_context *context,
                                uint_32 request_type, const void *payload,
                                uint_32 payload_size, uint_32 *request_id);
int_32 poudland_v1_wait_response(struct poudland_v1_context *context,
                                uint_32 request_id, int_32 timeout_ms,
                                struct poudland_v1_message *response);

/* Connect uses one monotonic deadline for service discovery and HELLO. */
int_32 poudland_v1_connect(struct poudland_v1_context *context,
                          const char *service, int_32 timeout_ms,
                          uint_32 client_capabilities);
/* Synchronous wrappers do not expose their request ID. A transport wait
 * timeout therefore closes and resets the context instead of leaving an
 * unreachable pending request. A valid ERROR response consumes its pending
 * request and leaves the context connected, including status -ETIMEDOUT. */
int_32 poudland_v1_window_create(
    struct poudland_v1_context *context,
    const struct poudland_v1_window_new *request, int_32 timeout_ms,
    struct poudland_v1_window_init *result);
int_32 poudland_v1_window_close(struct poudland_v1_context *context,
                               uint_32 window_id, int_32 timeout_ms);
int_32 poudland_v1_next_event(struct poudland_v1_context *context,
                             int_32 timeout_ms,
                             struct poudland_v1_message *event);
int_32 poudland_v1_disconnect(struct poudland_v1_context *context);

#endif
