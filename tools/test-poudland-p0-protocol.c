#include "../core/apps/poudland_p0/protocol.h"

#include <frog/errno.h>

struct test_message {
        struct poudland_v1_header header;
        uint_8 payload[POUDLAND_V1_P0_PAYLOAD_MAX];
};

static void header_prepare(struct test_message *message, uint_32 type,
                           uint_32 request_id, uint_32 payload_size)
{
        message->header.magic = POUDLAND_V1_MAGIC;
        message->header.version = POUDLAND_V1_VERSION;
        message->header.header_size = POUDLAND_V1_HEADER_SIZE;
        message->header.type = type;
        message->header.request_id = request_id;
        message->header.payload_size = payload_size;
}

static int_32 send_hello(struct poudland_p0_protocol *protocol,
                         uint_32 peer_id, uint_32 request_id,
                         uint_16 min_version, uint_16 max_version,
                         struct poudland_p0_protocol_result *result)
{
        struct test_message message;
        struct poudland_v1_hello *hello =
            (struct poudland_v1_hello *) message.payload;

        header_prepare(&message, POUDLAND_V1_MSG_HELLO, request_id,
                       sizeof(*hello));
        hello->min_version = min_version;
        hello->max_version = max_version;
        hello->capabilities = 0;
        return poudland_p0_protocol_handle_data(
            protocol, peer_id, &message,
            POUDLAND_V1_HEADER_SIZE + sizeof(*hello), result);
}

static int_32 send_new(struct poudland_p0_protocol *protocol,
                       uint_32 peer_id, uint_32 request_id,
                       int_32 x, int_32 y, uint_32 width, uint_32 height,
                       uint_32 color,
                       struct poudland_p0_protocol_result *result)
{
        struct test_message message;
        struct poudland_v1_window_new *window =
            (struct poudland_v1_window_new *) message.payload;

        header_prepare(&message, POUDLAND_V1_MSG_WINDOW_NEW, request_id,
                       sizeof(*window));
        window->x = x;
        window->y = y;
        window->width = width;
        window->height = height;
        window->xrgb8888 = color;
        return poudland_p0_protocol_handle_data(
            protocol, peer_id, &message,
            POUDLAND_V1_HEADER_SIZE + sizeof(*window), result);
}

static int_32 send_close(struct poudland_p0_protocol *protocol,
                         uint_32 peer_id, uint_32 request_id,
                         uint_32 window_id,
                         struct poudland_p0_protocol_result *result)
{
        struct test_message message;
        struct poudland_v1_window_close *close =
            (struct poudland_v1_window_close *) message.payload;

        header_prepare(&message, POUDLAND_V1_MSG_WINDOW_CLOSE, request_id,
                       sizeof(*close));
        close->window_id = window_id;
        return poudland_p0_protocol_handle_data(
            protocol, peer_id, &message,
            POUDLAND_V1_HEADER_SIZE + sizeof(*close), result);
}

static bool reply_is(const struct poudland_p0_protocol_result *result,
                     uint_32 type, uint_32 request_id,
                     uint_32 payload_size)
{
        const struct poudland_v1_header *header =
            (const struct poudland_v1_header *) result->reply;

        return result->reply_size == POUDLAND_V1_HEADER_SIZE + payload_size &&
               header->magic == POUDLAND_V1_MAGIC &&
               header->version == POUDLAND_V1_VERSION &&
               header->header_size == POUDLAND_V1_HEADER_SIZE &&
               header->type == type && header->request_id == request_id &&
               header->payload_size == payload_size;
}

static bool error_is(const struct poudland_p0_protocol_result *result,
                     uint_32 request_id, int_32 status,
                     uint_32 failed_type)
{
        const struct poudland_v1_error *error =
            (const struct poudland_v1_error *)
                (result->reply + POUDLAND_V1_HEADER_SIZE);

        return reply_is(result, POUDLAND_V1_MSG_ERROR, request_id,
                        sizeof(*error)) &&
               error->status == status && error->failed_type == failed_type;
}

static uint_32 created_id(
    const struct poudland_p0_protocol_result *result)
{
        const struct poudland_v1_window_init *window =
            (const struct poudland_v1_window_init *)
                (result->reply + POUDLAND_V1_HEADER_SIZE);

        return window->window_id;
}

static int hello_welcome(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_protocol_result result;
        const struct poudland_v1_welcome *welcome;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        if (send_hello(&protocol, 7, 41, 1, 1, &result) != 0)
                return 1;
        welcome = (const struct poudland_v1_welcome *)
            (result.reply + POUDLAND_V1_HEADER_SIZE);
        if (!reply_is(&result, POUDLAND_V1_MSG_WELCOME, 41,
                      sizeof(*welcome)) ||
            welcome->selected_version != POUDLAND_V1_VERSION ||
            welcome->reserved != 0 || welcome->display_width != 1024 ||
            welcome->display_height != 768 || welcome->capabilities != 0)
                return 2;
        return 0;
}

static int create_close_ownership_and_errors(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_protocol_result result;
        const struct poudland_v1_window_init *initialized;
        const struct poudland_v1_window_closed *closed;
        uint_32 first_id;
        uint_32 second_id;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        if (send_new(&protocol, 10, 1, 1, 2, 3, 4, 5, &result) != 0 ||
            !error_is(&result, 1, -EPROTO, POUDLAND_V1_MSG_WINDOW_NEW))
                return 10;
        if (send_hello(&protocol, 12, 2, 2, 2, &result) != 0 ||
            !error_is(&result, 2, -EPROTONOSUPPORT,
                      POUDLAND_V1_MSG_HELLO) ||
            !result.terminate_session)
                return 11;
        if (send_hello(&protocol, 10, 3, 1, 1, &result) != 0 ||
            !reply_is(&result, POUDLAND_V1_MSG_WELCOME, 3,
                      sizeof(struct poudland_v1_welcome)))
                return 12;
        if (send_new(&protocol, 10, 4, 100, 110, 200, 160,
                     0x00cc5533U, &result) != 0 ||
            !reply_is(&result, POUDLAND_V1_MSG_WINDOW_INIT, 4,
                      sizeof(*initialized)) || result.damage_count != 1)
                return 13;
        initialized = (const struct poudland_v1_window_init *)
            (result.reply + POUDLAND_V1_HEADER_SIZE);
        first_id = initialized->window_id;
        if (first_id == 0 || initialized->x != 100 || initialized->y != 110 ||
            initialized->width != 200 || initialized->height != 160 ||
            initialized->xrgb8888 != 0x00cc5533U ||
            result.damages[0].x != 100 || result.damages[0].y != 110 ||
            result.damages[0].width != 200 ||
            result.damages[0].height != 160)
                return 14;
        if (send_new(&protocol, 10, 5, 220, 200, 320, 240,
                     0x00339966U, &result) != 0)
                return 15;
        second_id = created_id(&result);
        if (second_id == 0 || second_id == first_id ||
            protocol.window_count != 2)
                return 16;
        if (send_hello(&protocol, 11, 6, 1, 1, &result) != 0 ||
            send_close(&protocol, 11, 7, first_id, &result) != 0 ||
            !error_is(&result, 7, -EPERM, POUDLAND_V1_MSG_WINDOW_CLOSE))
                return 17;
        if (send_close(&protocol, 10, 8, second_id, &result) != 0 ||
            !reply_is(&result, POUDLAND_V1_MSG_WINDOW_CLOSED, 8,
                      sizeof(*closed)) || result.damage_count != 1)
                return 18;
        closed = (const struct poudland_v1_window_closed *)
            (result.reply + POUDLAND_V1_HEADER_SIZE);
        if (closed->window_id != second_id || protocol.window_count != 1 ||
            result.damages[0].x != 220 || result.damages[0].y != 200)
                return 19;
        if (send_close(&protocol, 10, 9, second_id, &result) != 0 ||
            !error_is(&result, 9, -ENOENT,
                      POUDLAND_V1_MSG_WINDOW_CLOSE))
                return 20;
        if (send_close(&protocol, 10, 10, 0, &result) != 0 ||
            !error_is(&result, 10, -EINVAL,
                      POUDLAND_V1_MSG_WINDOW_CLOSE))
                return 21;
        if (send_new(&protocol, 10, 11, 0, 0, 0, 1, 0, &result) != 0 ||
            !error_is(&result, 11, -EINVAL,
                      POUDLAND_V1_MSG_WINDOW_NEW) ||
            send_new(&protocol, 10, 12, 0, 0, 4097, 1, 0, &result) != 0 ||
            !error_is(&result, 12, -EINVAL,
                      POUDLAND_V1_MSG_WINDOW_NEW) ||
            send_new(&protocol, 10, 13, 0x7fffffff, 0, 2, 1, 0,
                     &result) != 0 ||
            !error_is(&result, 13, -EOVERFLOW,
                      POUDLAND_V1_MSG_WINDOW_NEW))
                return 22;
        return 0;
}

static int limits_and_disconnect(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_protocol_result result;
        uint_32 peer;
        uint_32 index;
        uint_32 request_id = 1;
        uint_32 retained_id;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        for (peer = 1; peer <= 4; ++peer) {
                if (send_hello(&protocol, peer, request_id++, 1, 1,
                               &result) != 0)
                        return 30;
                for (index = 0; index < 16; ++index) {
                        if (send_new(&protocol, peer, request_id++,
                                     (int_32) index, (int_32) peer,
                                     10, 10, peer, &result) != 0 ||
                            !reply_is(&result,
                                      POUDLAND_V1_MSG_WINDOW_INIT,
                                      request_id - 1,
                                      sizeof(struct poudland_v1_window_init)))
                                return 31;
                }
                if (peer == 1 &&
                    (send_new(&protocol, peer, request_id++, 0, 0,
                              10, 10, 0, &result) != 0 ||
                     !error_is(&result, request_id - 1, -ENOSPC,
                               POUDLAND_V1_MSG_WINDOW_NEW)))
                        return 32;
        }
        retained_id = protocol.windows[0].id;
        if (protocol.window_count != 64 || retained_id == 0 ||
            send_hello(&protocol, 5, request_id++, 1, 1, &result) != 0 ||
            send_new(&protocol, 5, request_id++, 0, 0, 10, 10, 0,
                     &result) != 0 ||
            !error_is(&result, request_id - 1, -ENOSPC,
                      POUDLAND_V1_MSG_WINDOW_NEW))
                return 33;
        poudland_p0_protocol_handle_disconnect(&protocol, 1, &result);
        if (protocol.window_count != 48 || result.reply_size != 0 ||
            result.damage_count != 16)
                return 34;
        if (send_close(&protocol, 2, request_id++, retained_id, &result) != 0 ||
            !error_is(&result, request_id - 1, -ENOENT,
                      POUDLAND_V1_MSG_WINDOW_CLOSE))
                return 35;
        if (send_hello(&protocol, 1, request_id++, 1, 1, &result) != 0 ||
            send_new(&protocol, 1, request_id++, 0, 0, 10, 10, 0,
                     &result) != 0 || created_id(&result) <= 64)
                return 36;
        return 0;
}

static int malformed_frame(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_protocol_result result;
        struct test_message message;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        header_prepare(&message, POUDLAND_V1_MSG_HELLO, 77,
                       sizeof(struct poudland_v1_hello));
        message.header.header_size++;
        if (poudland_p0_protocol_handle_data(
                &protocol, 1, &message,
                POUDLAND_V1_HEADER_SIZE + sizeof(struct poudland_v1_hello),
                &result) != 0 ||
            !error_is(&result, 77, -EPROTO, POUDLAND_V1_MSG_HELLO))
                return 40;
        return 0;
}

static int decoder_boundaries(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_protocol_result result;
        struct test_message message;
        struct poudland_v1_hello *hello =
            (struct poudland_v1_hello *) message.payload;
        int_32 status;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        result.reply_size = 99;
        status = poudland_p0_protocol_handle_data(
            &protocol, 1, &message, POUDLAND_V1_HEADER_SIZE - 1, &result);
        if (status != -EPROTO || result.reply_size != 0)
                return 50;

        header_prepare(&message, POUDLAND_V1_MSG_HELLO, 0,
                       sizeof(*hello));
        hello->min_version = 1;
        hello->max_version = 1;
        hello->capabilities = 0;
        result.reply_size = 99;
        status = poudland_p0_protocol_handle_data(
            &protocol, 1, &message,
            POUDLAND_V1_HEADER_SIZE + sizeof(*hello), &result);
        if (status != -EPROTO || result.reply_size != 0)
                return 51;

        header_prepare(&message, POUDLAND_V1_MSG_HELLO, 1,
                       sizeof(*hello));
        message.header.version = 2;
        if (poudland_p0_protocol_handle_data(
                &protocol, 1, &message,
                POUDLAND_V1_HEADER_SIZE + sizeof(*hello), &result) != 0 ||
            !error_is(&result, 1, -EPROTONOSUPPORT,
                      POUDLAND_V1_MSG_HELLO) ||
            !result.terminate_session)
                return 52;

        header_prepare(&message, 0x55aaU, 2, 0);
        if (poudland_p0_protocol_handle_data(
                &protocol, 1, &message, POUDLAND_V1_HEADER_SIZE,
                &result) != 0 ||
            !error_is(&result, 2, -EPROTO, 0x55aaU))
                return 53;

        header_prepare(&message, POUDLAND_V1_MSG_HELLO, 3,
                       sizeof(*hello) - 1U);
        if (poudland_p0_protocol_handle_data(
                &protocol, 1, &message,
                POUDLAND_V1_HEADER_SIZE + sizeof(*hello) - 1U,
                &result) != 0 ||
            !error_is(&result, 3, -EPROTO, POUDLAND_V1_MSG_HELLO))
                return 54;

        header_prepare(&message, POUDLAND_V1_MSG_HELLO, 4,
                       sizeof(*hello));
        if (poudland_p0_protocol_handle_data(
                &protocol, 1, &message,
                POUDLAND_V1_HEADER_SIZE + sizeof(*hello) - 1U,
                &result) != 0 ||
            !error_is(&result, 4, -EPROTO, POUDLAND_V1_MSG_HELLO))
                return 55;

        if (send_hello(&protocol, 1, 5, 1, 1, &result) != 0 ||
            !reply_is(&result, POUDLAND_V1_MSG_WELCOME, 5,
                      sizeof(struct poudland_v1_welcome)) ||
            send_hello(&protocol, 1, 6, 1, 1, &result) != 0 ||
            !error_is(&result, 6, -EPROTO, POUDLAND_V1_MSG_HELLO))
                return 56;

        if (send_new(&protocol, 1, 7, -100, -200, 10, 20,
                     0x00112233U, &result) != 0 ||
            !reply_is(&result, POUDLAND_V1_MSG_WINDOW_INIT, 7,
                      sizeof(struct poudland_v1_window_init)))
                return 57;
        return 0;
}

static int terminal_version_cleanup(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_protocol_result result;
        struct test_message message;
        struct poudland_v1_window_close *close =
            (struct poudland_v1_window_close *) message.payload;
        uint_32 window_id;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        if (send_hello(&protocol, 21, 1, 1, 1, &result) != 0 ||
            send_new(&protocol, 21, 2, 10, 20, 30, 40,
                     0x00112233U, &result) != 0)
                return 70;
        window_id = created_id(&result);
        header_prepare(&message, POUDLAND_V1_MSG_WINDOW_CLOSE, 3,
                       sizeof(*close));
        message.header.version = 2;
        close->window_id = window_id;
        if (poudland_p0_protocol_handle_data(
                &protocol, 21, &message,
                POUDLAND_V1_HEADER_SIZE + sizeof(*close), &result) != 0 ||
            !error_is(&result, 3, -EPROTONOSUPPORT,
                      POUDLAND_V1_MSG_WINDOW_CLOSE) ||
            !result.terminate_session || protocol.window_count != 0 ||
            result.damage_count != 1 || result.damages[0].x != 10 ||
            result.damages[0].y != 20 || result.damages[0].width != 30 ||
            result.damages[0].height != 40)
                return 71;
        return 0;
}

static int session_and_id_boundaries(void)
{
        struct poudland_p0_protocol protocol;
        struct poudland_p0_protocol_result result;
        uint_32 peer;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        for (peer = 1; peer <= POUDLAND_P0_PROTOCOL_SESSION_MAX; ++peer) {
                if (send_hello(&protocol, peer, peer, 1, 1, &result) != 0 ||
                    !reply_is(&result, POUDLAND_V1_MSG_WELCOME, peer,
                              sizeof(struct poudland_v1_welcome)))
                        return 60;
        }
        if (send_hello(&protocol, POUDLAND_P0_PROTOCOL_SESSION_MAX + 1U,
                       99, 1, 1, &result) != 0 ||
            !error_is(&result, 99, -ENOSPC, POUDLAND_V1_MSG_HELLO))
                return 61;

        poudland_p0_protocol_init(&protocol, 1024, 768);
        if (send_hello(&protocol, 1, 1, 1, 1, &result) != 0)
                return 62;
        protocol.next_window_id = 0xffffffffU;
        if (send_new(&protocol, 1, 2, 1, 2, 3, 4, 5, &result) != 0 ||
            !reply_is(&result, POUDLAND_V1_MSG_WINDOW_INIT, 2,
                      sizeof(struct poudland_v1_window_init)) ||
            created_id(&result) != 0xffffffffU ||
            protocol.next_window_id != 0)
                return 63;
        if (send_close(&protocol, 1, 3, 0xffffffffU, &result) != 0 ||
            !reply_is(&result, POUDLAND_V1_MSG_WINDOW_CLOSED, 3,
                      sizeof(struct poudland_v1_window_closed)) ||
            send_new(&protocol, 1, 4, 1, 2, 3, 4, 5, &result) != 0 ||
            !error_is(&result, 4, -ENOSPC,
                      POUDLAND_V1_MSG_WINDOW_NEW) ||
            protocol.window_count != 0)
                return 64;
        return 0;
}

int main(void)
{
        int result = hello_welcome();

        if (result != 0)
                return result;
        result = create_close_ownership_and_errors();
        if (result != 0)
                return result;
        result = limits_and_disconnect();
        if (result != 0)
                return result;
        result = malformed_frame();
        if (result != 0)
                return result;
        result = decoder_boundaries();
        if (result != 0)
                return result;
        result = session_and_id_boundaries();
        if (result != 0)
                return result;
        return terminal_version_cleanup();
}
