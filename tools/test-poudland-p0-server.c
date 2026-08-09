#include "../core/apps/poudland_p0/server.h"

#include <frog/errno.h>

#define FAKE_SENT_MAX 32U

struct fake_transport {
        bool blocked;
        frog_pkg_peer_id blocked_peer;
        uint_32 send_calls;
        uint_32 sent_count;
        uint_32 sent_types[FAKE_SENT_MAX];
        uint_32 sent_window_ids[FAKE_SENT_MAX];
        int_32 sent_screen_x[FAKE_SENT_MAX];
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
            fake->sent_count >= FAKE_SENT_MAX)
                return -EIO;
        fake->sent_types[fake->sent_count] = header->type;
        if (header->type == POUDLAND_V1_MSG_POINTER_EVENT) {
                const struct poudland_v1_pointer_event *pointer =
                    (const struct poudland_v1_pointer_event *)
                        ((const uint_8 *) payload + POUDLAND_V1_HEADER_SIZE);

                fake->sent_window_ids[fake->sent_count] =
                    pointer->window_id;
                fake->sent_screen_x[fake->sent_count] = pointer->screen_x;
        } else if (header->type == POUDLAND_V1_MSG_WINDOW_CONFIGURE) {
                const struct poudland_v1_window_configure *configure =
                    (const struct poudland_v1_window_configure *)
                        ((const uint_8 *) payload + POUDLAND_V1_HEADER_SIZE);

                fake->sent_window_ids[fake->sent_count] =
                    configure->window_id;
                fake->sent_screen_x[fake->sent_count] = configure->x;
        }
        fake->sent_count++;
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

static void prepare_hello_caps(struct test_frame *frame,
                               uint_32 request_id,
                               uint_32 capabilities)
{
        prepare_hello(frame, request_id);
        ((struct poudland_v1_hello *) frame->payload)->capabilities =
            capabilities;
}

static const struct poudland_v1_header *pending_header(
    const struct poudland_p0_server_peer *peer, uint_32 logical_index)
{
        uint_32 index = (peer->reply_head + logical_index) %
                        POUDLAND_P0_REPLY_QUEUE_MAX;

        return (const struct poudland_v1_header *) peer->replies[index].data;
}

static const struct poudland_v1_pointer_event *pending_pointer(
    const struct poudland_p0_server_peer *peer, uint_32 logical_index)
{
        return (const struct poudland_v1_pointer_event *)
            ((const uint_8 *) pending_header(peer, logical_index) +
             POUDLAND_V1_HEADER_SIZE);
}

static const struct poudland_v1_window_configure *pending_configure(
    const struct poudland_p0_server_peer *peer, uint_32 logical_index)
{
        return (const struct poudland_v1_window_configure *)
            ((const uint_8 *) pending_header(peer, logical_index) +
             POUDLAND_V1_HEADER_SIZE);
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

static void prepare_new_rect(struct test_frame *frame,
                             uint_32 request_id, int_32 x, int_32 y,
                             uint_32 width, uint_32 height)
{
        struct poudland_v1_window_new *window =
            (struct poudland_v1_window_new *) frame->payload;

        prepare_header(frame, POUDLAND_V1_MSG_WINDOW_NEW, request_id,
                       sizeof(*window));
        window->x = x;
        window->y = y;
        window->width = width;
        window->height = height;
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

static int replaceable_event_backpressure(void)
{
        const uint_32 interaction_caps = POUDLAND_V1_CAP_POINTER |
                                         POUDLAND_V1_CAP_KEYBOARD;
        struct poudland_p0_server server;
        struct poudland_p0_server_peer *peer;
        struct fake_transport fake = {
            .blocked_peer = 41,
        };
        struct frog_pkg_message message;
        struct test_frame frame;
        uint_32 first_id;
        uint_32 second_id;

        poudland_p0_server_state_init(&server, 1024, 768,
                                      fake_send, fake_damage, &fake);
        prepare_hello_caps(&frame, 1, interaction_caps);
        prepare_data(&message, 41, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 30;
        prepare_new_rect(&frame, 2, 100, 100, 200, 160);
        prepare_data(&message, 41, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 31;
        first_id = server.protocol.windows[0].id;
        prepare_new_rect(&frame, 3, 220, 200, 320, 240);
        prepare_data(&message, 41, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 32;
        second_id = server.protocol.windows[1].id;
        peer = &server.peers[0];
        fake.blocked = true;

        /* With no reliable barrier, a newer drag replaces the old slot. */
        if (!poudland_p0_server_handle_pointer(
                &server, 400, 210, POUDLAND_P0_BUTTON_LEFT) ||
            !poudland_p0_server_handle_pointer(
                &server, 410, 220, POUDLAND_P0_BUTTON_LEFT) ||
            !poudland_p0_server_handle_pointer(
                &server, 420, 230, POUDLAND_P0_BUTTON_LEFT) ||
            peer->reply_count != 3 ||
            pending_header(peer, 0)->type !=
                POUDLAND_V1_MSG_POINTER_EVENT ||
            pending_pointer(peer, 0)->type != POUDLAND_V1_POINTER_ENTER ||
            pending_pointer(peer, 0)->window_id != second_id ||
            pending_pointer(peer, 1)->type != POUDLAND_V1_POINTER_DOWN ||
            pending_pointer(peer, 2)->type != POUDLAND_V1_POINTER_DRAG ||
            pending_pointer(peer, 2)->window_id != second_id ||
            pending_pointer(peer, 2)->screen_x != 420)
                return 33;

        /* A reliable key is a barrier: remove the stale drag and append the
         * replacement behind KEY, yielding DOWN, KEY, DRAG(new). */
        if (!poudland_p0_server_handle_key(&server, 'a') ||
            !poudland_p0_server_handle_pointer(
                &server, 430, 240, POUDLAND_P0_BUTTON_LEFT) ||
            peer->reply_count != 4 ||
            pending_pointer(peer, 0)->type != POUDLAND_V1_POINTER_ENTER ||
            pending_pointer(peer, 1)->type != POUDLAND_V1_POINTER_DOWN ||
            pending_header(peer, 2)->type != POUDLAND_V1_MSG_KEY_EVENT ||
            pending_pointer(peer, 3)->type != POUDLAND_V1_POINTER_DRAG ||
            pending_pointer(peer, 3)->window_id != second_id ||
            pending_pointer(peer, 3)->screen_x != 430)
                return 34;

        fake.blocked = false;
        prepare_control(&message, 41, FROG_PKG_WRITABLE);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            peer->reply_count != 0 || fake.sent_count != 7 ||
            fake.sent_window_ids[3] != second_id ||
            fake.sent_window_ids[4] != second_id ||
            fake.sent_types[5] != POUDLAND_V1_MSG_KEY_EVENT ||
            fake.sent_window_ids[6] != second_id ||
            fake.sent_screen_x[6] != 430 ||
            !poudland_p0_server_handle_pointer(&server, 430, 240, 0))
                return 35;

        /* Replaceable records are keyed by both type and window. */
        fake.blocked = true;
        if (!poudland_p0_server_handle_pointer(&server, 400, 240, 0) ||
            !poudland_p0_server_handle_pointer(&server, 110, 110, 0) ||
            peer->reply_count != 4 ||
            pending_pointer(peer, 0)->type != POUDLAND_V1_POINTER_MOVE ||
            pending_pointer(peer, 0)->window_id != second_id ||
            pending_pointer(peer, 1)->type != POUDLAND_V1_POINTER_LEAVE ||
            pending_pointer(peer, 1)->window_id != second_id ||
            pending_pointer(peer, 2)->type != POUDLAND_V1_POINTER_ENTER ||
            pending_pointer(peer, 2)->window_id != first_id ||
            pending_pointer(peer, 3)->type != POUDLAND_V1_POINTER_MOVE ||
            pending_pointer(peer, 3)->window_id != first_id)
                return 36;
        return 0;
}

static int configure_and_move_replacement(void)
{
        const uint_32 all_caps = POUDLAND_V1_CAP_CONFIGURE |
                                 POUDLAND_V1_CAP_POINTER |
                                 POUDLAND_V1_CAP_KEYBOARD;
        struct poudland_p0_server server;
        struct poudland_p0_server_peer *peer;
        struct fake_transport fake = {
            .blocked_peer = 51,
        };
        struct frog_pkg_message message;
        struct test_frame frame;
        uint_32 window_id;

        poudland_p0_server_state_init(&server, 1024, 768,
                                      fake_send, fake_damage, &fake);
        prepare_hello_caps(&frame, 1, all_caps);
        prepare_data(&message, 51, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 40;
        prepare_new_rect(&frame, 2, 220, 200, 320, 240);
        prepare_data(&message, 51, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 41;
        window_id = server.protocol.windows[0].id;
        peer = &server.peers[0];
        fake.blocked = true;
        if (!poudland_p0_server_handle_pointer(
                &server, 400, 210, POUDLAND_P0_BUTTON_LEFT) ||
            !poudland_p0_server_handle_pointer(
                &server, 410, 220, POUDLAND_P0_BUTTON_LEFT) ||
            !poudland_p0_server_handle_pointer(
                &server, 420, 230, POUDLAND_P0_BUTTON_LEFT) ||
            peer->reply_count != 4 ||
            pending_header(peer, 2)->type !=
                POUDLAND_V1_MSG_WINDOW_CONFIGURE ||
            pending_configure(peer, 2)->window_id != window_id ||
            pending_configure(peer, 2)->x != 240 ||
            pending_pointer(peer, 3)->type != POUDLAND_V1_POINTER_DRAG ||
            pending_pointer(peer, 3)->screen_x != 420)
                return 42;
        fake.blocked = false;
        prepare_control(&message, 51, FROG_PKG_WRITABLE);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            !poudland_p0_server_handle_pointer(&server, 420, 230, 0))
                return 43;
        fake.blocked = true;
        if (!poudland_p0_server_handle_pointer(&server, 250, 230, 0) ||
            !poudland_p0_server_handle_pointer(&server, 260, 240, 0) ||
            peer->reply_count != 1 ||
            pending_pointer(peer, 0)->type != POUDLAND_V1_POINTER_MOVE ||
            pending_pointer(peer, 0)->window_id != window_id ||
            pending_pointer(peer, 0)->screen_x != 260)
                return 44;
        return 0;
}

static int full_replaceable_queue_is_peer_local(void)
{
        struct poudland_p0_server server;
        struct fake_transport fake = {
            .blocked = true,
            .blocked_peer = 61,
        };
        struct frog_pkg_message message;
        struct test_frame frame;
        uint_32 index;

        poudland_p0_server_state_init(&server, 1024, 768,
                                      fake_send, fake_damage, &fake);
        prepare_hello_caps(&frame, 1, POUDLAND_V1_CAP_POINTER);
        prepare_data(&message, 61, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 50;
        for (index = 0; index < POUDLAND_P0_REPLY_QUEUE_MAX - 1U;
             ++index) {
                prepare_new(&frame, index + 2U, (int_32) (100U + index));
                prepare_data(&message, 61, &frame);
                if (!poudland_p0_server_handle_record(&server, &message))
                        return 51;
        }
        if (server.peers[0].reply_count != POUDLAND_P0_REPLY_QUEUE_MAX ||
            !poudland_p0_server_handle_pointer(&server, 120, 30, 0) ||
            !server.peers[0].tombstone ||
            server.peers[0].reply_count != 0 ||
            server.protocol.window_count != 0)
                return 52;

        /* Queue exhaustion isolates the unresponsive session; the server
         * continues accepting and serving another peer. */
        prepare_hello(&frame, 20);
        prepare_data(&message, 62, &frame);
        if (!poudland_p0_server_handle_record(&server, &message))
                return 53;
        prepare_new(&frame, 21, 300);
        prepare_data(&message, 62, &frame);
        if (!poudland_p0_server_handle_record(&server, &message) ||
            server.protocol.window_count != 1 ||
            !server.peers[1].active || server.peers[1].tombstone)
                return 54;
        return 0;
}

int main(void)
{
        int result = ordered_backpressure();

        poudland_p0_server_state_init(NULL, 0, 0, NULL, NULL, NULL);
        if (result != 0)
                return result;
        result = bounded_tombstone();
        if (result != 0)
                return result;
        result = terminal_error_delivery();
        if (result != 0)
                return result;
        result = replaceable_event_backpressure();
        if (result != 0)
                return result;
        result = configure_and_move_replacement();
        return result != 0 ? result :
               full_replaceable_queue_is_peer_local();
}
