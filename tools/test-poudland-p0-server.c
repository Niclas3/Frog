#include "../core/apps/poudland_p0/server.h"

#include <frog/errno.h>

struct fake_transport {
        bool blocked;
        frog_pkg_peer_id blocked_peer;
        uint_32 send_calls;
        uint_32 sent_count;
        uint_32 sent_types[POUDLAND_P0_REPLY_QUEUE_MAX];
        uint_32 damage_count;
};

struct test_frame {
        struct poudland_v1_header header;
        uint_8 payload[POUDLAND_V1_P0_PAYLOAD_MAX];
};

static int_32 fake_send(struct poudland_p0_server *server,
                        frog_pkg_peer_id peer_id, const void *payload,
                        uint_32 payload_size)
{
        struct fake_transport *fake = server->transport_context;
        const struct poudland_v1_header *header = payload;

        fake->send_calls++;
        if (fake->blocked && fake->blocked_peer == peer_id)
                return -EAGAIN;
        if (payload_size < POUDLAND_V1_HEADER_SIZE ||
            fake->sent_count >= POUDLAND_P0_REPLY_QUEUE_MAX)
                return -EIO;
        fake->sent_types[fake->sent_count++] = header->type;
        return 0;
}

static void fake_damage(struct poudland_p0_server *server,
                        struct poudland_p0_rect rect)
{
        struct fake_transport *fake = server->transport_context;

        if (rect.width > 0 && rect.height > 0)
                fake->damage_count++;
}

static void prepare_header(struct test_frame *frame, uint_32 type,
                           uint_32 request_id, uint_32 payload_size)
{
        frame->header.magic = POUDLAND_V1_MAGIC;
        frame->header.version = POUDLAND_V1_VERSION;
        frame->header.header_size = POUDLAND_V1_HEADER_SIZE;
        frame->header.type = type;
        frame->header.request_id = request_id;
        frame->header.payload_size = payload_size;
}

static void prepare_hello(struct test_frame *frame, uint_32 request_id)
{
        struct poudland_v1_hello *hello =
            (struct poudland_v1_hello *) frame->payload;

        prepare_header(frame, POUDLAND_V1_MSG_HELLO, request_id,
                       sizeof(*hello));
        hello->min_version = POUDLAND_V1_VERSION;
        hello->max_version = POUDLAND_V1_VERSION;
        hello->capabilities = 0;
}

static void prepare_new(struct test_frame *frame, uint_32 request_id,
                        int_32 x)
{
        struct poudland_v1_window_new *window =
            (struct poudland_v1_window_new *) frame->payload;

        prepare_header(frame, POUDLAND_V1_MSG_WINDOW_NEW, request_id,
                       sizeof(*window));
        window->x = x;
        window->y = 20;
        window->width = 30;
        window->height = 40;
        window->xrgb8888 = 0x00112233U;
}

static void prepare_data(struct frog_pkg_message *message,
                         frog_pkg_peer_id peer_id,
                         const struct test_frame *frame)
{
        uint_32 size = POUDLAND_V1_HEADER_SIZE +
                       frame->header.payload_size;
        const uint_8 *source = (const uint_8 *) frame;
        uint_32 index;

        message->peer_id = peer_id;
        message->event = FROG_PKG_DATA;
        message->payload_size = size;
        for (index = 0; index < size; ++index)
                message->payload[index] = source[index];
}

static void prepare_control(struct frog_pkg_message *message,
                            frog_pkg_peer_id peer_id, uint_32 event)
{
        message->peer_id = peer_id;
        message->event = event;
        message->payload_size = 0;
}

static int ordered_backpressure(void)
{
        struct poudland_p0_server server;
        struct fake_transport fake = {
            .blocked = true,
            .blocked_peer = 7,
        };
        struct frog_pkg_message message;
        struct test_frame frame;

        poudland_p0_server_state_init(&server, 1024, 768,
                                      fake_send, fake_damage, &fake);
        prepare_hello(&frame, 1);
        prepare_data(&message, 7, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].reply_count != 1 || fake.send_calls != 1)
                return 1;
        prepare_new(&frame, 2, 10);
        prepare_data(&message, 7, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].reply_count != 2 || fake.send_calls != 1 ||
            server.protocol.window_count != 1 || fake.damage_count != 1)
                return 2;

        prepare_control(&message, 8, FROG_PKG_WRITABLE);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].reply_count != 2 || fake.send_calls != 1)
                return 3;
        fake.blocked = false;
        prepare_control(&message, 7, FROG_PKG_WRITABLE);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].reply_count != 0 || fake.sent_count != 2 ||
            fake.sent_types[0] != POUDLAND_V1_MSG_WELCOME ||
            fake.sent_types[1] != POUDLAND_V1_MSG_WINDOW_INIT)
                return 4;

        prepare_control(&message, 7, FROG_PKG_DISCONNECT);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].active || server.protocol.window_count != 0 ||
            fake.damage_count != 2)
                return 5;
        return 0;
}

static int bounded_tombstone(void)
{
        struct poudland_p0_server server;
        struct fake_transport fake = {
            .blocked = true,
            .blocked_peer = 9,
        };
        struct frog_pkg_message message;
        struct test_frame frame;
        uint_32 index;

        poudland_p0_server_state_init(&server, 1024, 768,
                                      fake_send, fake_damage, &fake);
        prepare_hello(&frame, 1);
        prepare_data(&message, 9, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 10;
        for (index = 0; index < POUDLAND_P0_REPLY_QUEUE_MAX - 1U; ++index) {
                prepare_new(&frame, index + 2U, (int_32) index);
                prepare_data(&message, 9, &frame);
                if (!poudland_p0_server_handle_record(&server, &message))
                        return 11;
        }
        if (server.peers[0].reply_count != POUDLAND_P0_REPLY_QUEUE_MAX ||
            server.protocol.window_count !=
                POUDLAND_P0_REPLY_QUEUE_MAX - 1U)
                return 12;

        prepare_new(&frame, 20, 100);
        prepare_data(&message, 9, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            !server.peers[0].tombstone ||
            server.peers[0].reply_count != 0 ||
            server.protocol.window_count != 0 ||
            fake.damage_count != 2U *
                (POUDLAND_P0_REPLY_QUEUE_MAX - 1U))
                return 13;
        prepare_data(&message, 9, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.protocol.window_count != 0)
                return 14;
        prepare_control(&message, 9, FROG_PKG_DISCONNECT);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].active)
                return 15;
        return 0;
}

static int terminal_error_delivery(void)
{
        struct poudland_p0_server server;
        struct fake_transport fake = {
            .blocked = true,
            .blocked_peer = 31,
        };
        struct frog_pkg_message message;
        struct test_frame frame;
        struct poudland_v1_hello *hello =
            (struct poudland_v1_hello *) frame.payload;

        poudland_p0_server_state_init(&server, 1024, 768,
                                      fake_send, fake_damage, &fake);
        prepare_hello(&frame, 1);
        frame.header.version = 2;
        prepare_data(&message, 31, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            !server.peers[0].closing ||
            server.peers[0].reply_count != 1 ||
            server.peers[0].tombstone || server.protocol.window_count != 0)
                return 20;
        prepare_hello(&frame, 2);
        prepare_data(&message, 31, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].reply_count != 1)
                return 21;
        fake.blocked = false;
        prepare_control(&message, 31, FROG_PKG_WRITABLE);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            !server.peers[0].tombstone || server.peers[0].closing ||
            server.peers[0].reply_count != 0 || fake.sent_count != 1 ||
            fake.sent_types[0] != POUDLAND_V1_MSG_ERROR)
                return 22;
        prepare_control(&message, 31, FROG_PKG_DISCONNECT);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.peers[0].active)
                return 23;

        poudland_p0_server_state_init(&server, 1024, 768,
                                      fake_send, fake_damage, &fake);
        fake.blocked = true;
        fake.blocked_peer = 32;
        fake.send_calls = 0;
        fake.sent_count = 0;
        fake.damage_count = 0;
        prepare_hello(&frame, 1);
        prepare_data(&message, 32, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 24;
        prepare_new(&frame, 2, 10);
        prepare_data(&message, 32, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.protocol.window_count != 1 ||
            server.peers[0].reply_count != 2)
                return 25;
        prepare_header(&frame, POUDLAND_V1_MSG_HELLO, 3, sizeof(*hello));
        frame.header.version = 2;
        hello->min_version = 1;
        hello->max_version = 1;
        hello->capabilities = 0;
        prepare_data(&message, 32, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            !server.peers[0].closing ||
            server.peers[0].reply_count != 3 ||
            server.protocol.window_count != 0 || fake.damage_count != 2)
                return 26;
        prepare_new(&frame, 4, 50);
        prepare_data(&message, 32, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.protocol.window_count != 0 ||
            server.peers[0].reply_count != 3)
                return 27;
        fake.blocked = false;
        prepare_control(&message, 32, FROG_PKG_WRITABLE);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            !server.peers[0].tombstone || fake.sent_count != 3 ||
            fake.sent_types[0] != POUDLAND_V1_MSG_WELCOME ||
            fake.sent_types[1] != POUDLAND_V1_MSG_WINDOW_INIT ||
            fake.sent_types[2] != POUDLAND_V1_MSG_ERROR)
                return 28;
        return 0;
}

int main(void)
{
        int result = ordered_backpressure();

        poudland_p0_server_state_init(NULL, 0, 0, NULL, NULL, NULL);
        if (result != 0)
                return result;
        result = bounded_tombstone();
        return result != 0 ? result : terminal_error_delivery();
}
