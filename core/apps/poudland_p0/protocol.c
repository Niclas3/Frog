#include "protocol.h"

#include <frog/errno.h>

#define POUDLAND_P0_INT32_MAX 0x7fffffff

static void clear_bytes(void *data, uint_32 size)
{
        uint_8 *bytes = data;
        uint_32 index;

        for (index = 0; index < size; ++index)
                bytes[index] = 0;
}

static struct poudland_p0_protocol_session *find_session(
    struct poudland_p0_protocol *protocol, uint_32 peer_id)
{
        uint_32 index;

        for (index = 0; index < POUDLAND_P0_PROTOCOL_SESSION_MAX; ++index) {
                struct poudland_p0_protocol_session *session =
                    &protocol->sessions[index];

                if (session->active && session->peer_id == peer_id)
                        return session;
        }
        return NULL;
}

static struct poudland_p0_protocol_session *allocate_session(
    struct poudland_p0_protocol *protocol, uint_32 peer_id)
{
        uint_32 index;

        for (index = 0; index < POUDLAND_P0_PROTOCOL_SESSION_MAX; ++index) {
                struct poudland_p0_protocol_session *session =
                    &protocol->sessions[index];

                if (session->active)
                        continue;
                clear_bytes(session, sizeof(*session));
                session->active = true;
                session->peer_id = peer_id;
                return session;
        }
        return NULL;
}

static struct poudland_p0_protocol_window *find_window(
    struct poudland_p0_protocol *protocol, uint_32 window_id)
{
        uint_32 index;

        for (index = 0; index < POUDLAND_V1_SERVER_WINDOW_MAX; ++index) {
                struct poudland_p0_protocol_window *window =
                    &protocol->windows[index];

                if (window->active && window->id == window_id)
                        return window;
        }
        return NULL;
}

static struct poudland_p0_protocol_window *allocate_window(
    struct poudland_p0_protocol *protocol)
{
        uint_32 index;

        for (index = 0; index < POUDLAND_V1_SERVER_WINDOW_MAX; ++index) {
                if (!protocol->windows[index].active)
                        return &protocol->windows[index];
        }
        return NULL;
}

static void prepare_reply(struct poudland_p0_protocol_result *result,
                          uint_32 type, uint_32 request_id,
                          uint_32 payload_size)
{
        struct poudland_v1_header *header;

        clear_bytes(result, sizeof(*result));
        header = (struct poudland_v1_header *) result->reply;
        header->magic = POUDLAND_V1_MAGIC;
        header->version = POUDLAND_V1_VERSION;
        header->header_size = POUDLAND_V1_HEADER_SIZE;
        header->type = type;
        header->request_id = request_id;
        header->payload_size = payload_size;
        result->reply_size = POUDLAND_V1_HEADER_SIZE + payload_size;
}

static int_32 prepare_error(struct poudland_p0_protocol_result *result,
                            uint_32 request_id, uint_32 failed_type,
                            int_32 status)
{
        struct poudland_v1_error *error;

        prepare_reply(result, POUDLAND_V1_MSG_ERROR, request_id,
                      sizeof(*error));
        error = (struct poudland_v1_error *)
            (result->reply + POUDLAND_V1_HEADER_SIZE);
        error->status = status;
        error->failed_type = failed_type;
        return 0;
}

static int_32 prepare_terminal_error(
    struct poudland_p0_protocol_result *result, uint_32 request_id,
    uint_32 failed_type, int_32 status)
{
        prepare_error(result, request_id, failed_type, status);
        result->terminate_session = true;
        return 0;
}

static bool request_payload_size(uint_32 type, uint_32 *payload_size)
{
        switch (type) {
        case POUDLAND_V1_MSG_HELLO:
                *payload_size = sizeof(struct poudland_v1_hello);
                return true;
        case POUDLAND_V1_MSG_WINDOW_NEW:
                *payload_size = sizeof(struct poudland_v1_window_new);
                return true;
        case POUDLAND_V1_MSG_WINDOW_CLOSE:
                *payload_size = sizeof(struct poudland_v1_window_close);
                return true;
        default:
                return false;
        }
}

static int_32 validate_frame(const struct poudland_v1_header *header,
                             uint_32 message_size,
                             struct poudland_p0_protocol_result *result)
{
        uint_32 expected_payload_size;

        if (header->request_id == 0)
                return -EPROTO;
        if (header->magic != POUDLAND_V1_MAGIC ||
            header->header_size != POUDLAND_V1_HEADER_SIZE)
                return prepare_error(result, header->request_id,
                                     header->type, -EPROTO);
        if (header->version != POUDLAND_V1_VERSION)
                return prepare_terminal_error(
                    result, header->request_id, header->type,
                    -EPROTONOSUPPORT);
        if (!request_payload_size(header->type, &expected_payload_size) ||
            header->payload_size != expected_payload_size ||
            message_size != POUDLAND_V1_HEADER_SIZE +
                                expected_payload_size)
                return prepare_error(result, header->request_id,
                                     header->type, -EPROTO);
        return 1;
}

static int_32 handle_hello(struct poudland_p0_protocol *protocol,
                           uint_32 peer_id,
                           const struct poudland_v1_header *header,
                           const struct poudland_v1_hello *hello,
                           struct poudland_p0_protocol_result *result)
{
        struct poudland_p0_protocol_session *session;
        struct poudland_v1_welcome *welcome;

        if (hello->min_version > hello->max_version ||
            hello->min_version > POUDLAND_V1_VERSION ||
            hello->max_version < POUDLAND_V1_VERSION)
                return prepare_terminal_error(
                    result, header->request_id, header->type,
                    -EPROTONOSUPPORT);
        if (find_session(protocol, peer_id))
                return prepare_error(result, header->request_id,
                                     header->type, -EPROTO);
        session = allocate_session(protocol, peer_id);
        if (!session)
                return prepare_error(result, header->request_id,
                                     header->type, -ENOSPC);
        session->welcomed = true;
        prepare_reply(result, POUDLAND_V1_MSG_WELCOME,
                      header->request_id, sizeof(*welcome));
        welcome = (struct poudland_v1_welcome *)
            (result->reply + POUDLAND_V1_HEADER_SIZE);
        welcome->selected_version = POUDLAND_V1_VERSION;
        welcome->display_width = protocol->display_width;
        welcome->display_height = protocol->display_height;
        welcome->capabilities = 0;
        return 0;
}

static int_32 geometry_status(const struct poudland_v1_window_new *request)
{
        int_64 right;
        int_64 bottom;

        if (request->width == 0 || request->height == 0 ||
            request->width > 4096U || request->height > 4096U)
                return -EINVAL;
        right = (int_64) request->x + request->width;
        bottom = (int_64) request->y + request->height;
        if (right > POUDLAND_P0_INT32_MAX ||
            bottom > POUDLAND_P0_INT32_MAX)
                return -EOVERFLOW;
        return 0;
}

static int_32 handle_window_new(
    struct poudland_p0_protocol *protocol,
    struct poudland_p0_protocol_session *session,
    const struct poudland_v1_header *header,
    const struct poudland_v1_window_new *request,
    struct poudland_p0_protocol_result *result)
{
        struct poudland_p0_protocol_window *window;
        struct poudland_v1_window_init *initialized;
        int_32 status = geometry_status(request);

        if (status != 0)
                return prepare_error(result, header->request_id,
                                     header->type, status);
        if (session->window_count >= POUDLAND_V1_SESSION_WINDOW_MAX ||
            protocol->window_count >= POUDLAND_V1_SERVER_WINDOW_MAX ||
            protocol->next_window_id == 0)
                return prepare_error(result, header->request_id,
                                     header->type, -ENOSPC);
        window = allocate_window(protocol);
        if (!window)
                return prepare_error(result, header->request_id,
                                     header->type, -ENOSPC);
        clear_bytes(window, sizeof(*window));
        window->active = true;
        window->id = protocol->next_window_id++;
        window->owner_peer_id = session->peer_id;
        window->bounds.x = request->x;
        window->bounds.y = request->y;
        window->bounds.width = (int_32) request->width;
        window->bounds.height = (int_32) request->height;
        window->color = request->xrgb8888;
        session->window_count++;
        protocol->window_count++;

        prepare_reply(result, POUDLAND_V1_MSG_WINDOW_INIT,
                      header->request_id, sizeof(*initialized));
        initialized = (struct poudland_v1_window_init *)
            (result->reply + POUDLAND_V1_HEADER_SIZE);
        initialized->window_id = window->id;
        initialized->x = request->x;
        initialized->y = request->y;
        initialized->width = request->width;
        initialized->height = request->height;
        initialized->xrgb8888 = request->xrgb8888;
        result->damage_count = 1;
        result->damages[0] = window->bounds;
        return 0;
}

static int_32 handle_window_close(
    struct poudland_p0_protocol *protocol,
    struct poudland_p0_protocol_session *session,
    const struct poudland_v1_header *header,
    const struct poudland_v1_window_close *request,
    struct poudland_p0_protocol_result *result)
{
        struct poudland_p0_protocol_window *window;
        struct poudland_v1_window_closed *closed;

        if (request->window_id == 0)
                return prepare_error(result, header->request_id,
                                     header->type, -EINVAL);
        window = find_window(protocol, request->window_id);
        if (!window)
                return prepare_error(result, header->request_id,
                                     header->type, -ENOENT);
        if (window->owner_peer_id != session->peer_id)
                return prepare_error(result, header->request_id,
                                     header->type, -EPERM);

        prepare_reply(result, POUDLAND_V1_MSG_WINDOW_CLOSED,
                      header->request_id, sizeof(*closed));
        closed = (struct poudland_v1_window_closed *)
            (result->reply + POUDLAND_V1_HEADER_SIZE);
        closed->window_id = window->id;
        result->damage_count = 1;
        result->damages[0] = window->bounds;
        clear_bytes(window, sizeof(*window));
        session->window_count--;
        protocol->window_count--;
        return 0;
}

void poudland_p0_protocol_init(struct poudland_p0_protocol *protocol,
                               uint_32 display_width,
                               uint_32 display_height)
{
        clear_bytes(protocol, sizeof(*protocol));
        protocol->display_width = display_width;
        protocol->display_height = display_height;
        protocol->next_window_id = 1;
}

static void destroy_peer_windows(
    struct poudland_p0_protocol *protocol, uint_32 peer_id,
    struct poudland_p0_protocol_result *result)
{
        struct poudland_p0_protocol_session *session =
            find_session(protocol, peer_id);
        uint_32 index;

        for (index = 0; index < POUDLAND_V1_SERVER_WINDOW_MAX; ++index) {
                struct poudland_p0_protocol_window *window =
                    &protocol->windows[index];

                if (!window->active || window->owner_peer_id != peer_id)
                        continue;
                if (result->damage_count < POUDLAND_P0_PROTOCOL_DAMAGE_MAX)
                        result->damages[result->damage_count++] =
                            window->bounds;
                clear_bytes(window, sizeof(*window));
                protocol->window_count--;
        }
        if (session)
                clear_bytes(session, sizeof(*session));
}

int_32 poudland_p0_protocol_handle_data(
    struct poudland_p0_protocol *protocol, uint_32 peer_id,
    const void *message, uint_32 message_size,
    struct poudland_p0_protocol_result *result)
{
        const struct poudland_v1_header *header;
        const uint_8 *payload;
        struct poudland_p0_protocol_session *session;
        int_32 status;

        if (!protocol || peer_id == 0 || !message || !result)
                return -EINVAL;
        clear_bytes(result, sizeof(*result));
        if (message_size < POUDLAND_V1_HEADER_SIZE)
                return -EPROTO;
        header = message;
        status = validate_frame(header, message_size, result);
        if (status <= 0) {
                if (result->terminate_session)
                        destroy_peer_windows(protocol, peer_id, result);
                return status;
        }
        payload = (const uint_8 *) message + POUDLAND_V1_HEADER_SIZE;
        if (header->type == POUDLAND_V1_MSG_HELLO) {
                status = handle_hello(
                    protocol, peer_id, header,
                    (const struct poudland_v1_hello *) payload, result);
                if (result->terminate_session)
                        destroy_peer_windows(protocol, peer_id, result);
                return status;
        }

        session = find_session(protocol, peer_id);
        if (!session || !session->welcomed)
                return prepare_error(result, header->request_id,
                                     header->type, -EPROTO);
        if (header->type == POUDLAND_V1_MSG_WINDOW_NEW)
                return handle_window_new(
                    protocol, session, header,
                    (const struct poudland_v1_window_new *) payload, result);
        return handle_window_close(
            protocol, session, header,
            (const struct poudland_v1_window_close *) payload, result);
}

void poudland_p0_protocol_handle_disconnect(
    struct poudland_p0_protocol *protocol, uint_32 peer_id,
    struct poudland_p0_protocol_result *result)
{
        if (!result)
                return;
        clear_bytes(result, sizeof(*result));
        if (!protocol || peer_id == 0)
                return;
        destroy_peer_windows(protocol, peer_id, result);
}
