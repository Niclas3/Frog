#include "server.h"

#include <frog/errno.h>

#ifndef POUDLAND_P0_SERVER_HOST_TEST
#include "poudland_p0.h"

#include <frog/poll.h>
#include <frog/syscall.h>
#include <input/mouse.h>
#endif

static void clear_bytes(void *data, uint_32 size)
{
        uint_8 *bytes = data;
        uint_32 index;

        for (index = 0; index < size; ++index)
                bytes[index] = 0;
}

static void copy_bytes(void *destination, const void *source,
                       uint_32 size)
{
        uint_8 *to = destination;
        const uint_8 *from = source;
        uint_32 index;

        for (index = 0; index < size; ++index)
                to[index] = from[index];
}

static struct poudland_p0_server_peer *find_peer(
    struct poudland_p0_server *server, frog_pkg_peer_id peer_id)
{
        uint_32 index;

        for (index = 0; index < POUDLAND_P0_PROTOCOL_SESSION_MAX; ++index) {
                struct poudland_p0_server_peer *peer =
                    &server->peers[index];

                if (peer->active && peer->peer_id == peer_id)
                        return peer;
        }
        return NULL;
}

static struct poudland_p0_server_peer *allocate_peer(
    struct poudland_p0_server *server, frog_pkg_peer_id peer_id)
{
        uint_32 index;

        for (index = 0; index < POUDLAND_P0_PROTOCOL_SESSION_MAX; ++index) {
                struct poudland_p0_server_peer *peer =
                    &server->peers[index];

                if (peer->active)
                        continue;
                clear_bytes(peer, sizeof(*peer));
                peer->active = true;
                peer->peer_id = peer_id;
                return peer;
        }
        return NULL;
}

static bool protocol_peer_welcomed(
    const struct poudland_p0_protocol *protocol,
    frog_pkg_peer_id peer_id)
{
        uint_32 index;

        for (index = 0; index < POUDLAND_P0_PROTOCOL_SESSION_MAX; ++index) {
                const struct poudland_p0_protocol_session *session =
                    &protocol->sessions[index];

                if (session->active && session->welcomed &&
                    session->peer_id == peer_id)
                        return true;
        }
        return false;
}

static void record_delivered_reply(struct poudland_p0_server *server,
                                   struct poudland_p0_server_peer *peer,
                                   const void *data, uint_32 size)
{
        const struct poudland_v1_header *header = data;

        if (size < POUDLAND_V1_HEADER_SIZE ||
            header->type != POUDLAND_V1_MSG_WELCOME ||
            !protocol_peer_welcomed(&server->protocol, peer->peer_id))
                return;
        peer->welcomed = true;
        server->served_client = true;
}

enum poudland_p0_server_lifecycle poudland_p0_server_lifecycle(
    const struct poudland_p0_server *server, bool startup_expired)
{
        uint_32 index;

        if (!server)
                return POUDLAND_P0_SERVER_STOP_STARTUP_TIMEOUT;
        for (index = 0; index < POUDLAND_P0_PROTOCOL_SESSION_MAX; ++index) {
                const struct poudland_p0_server_peer *peer =
                    &server->peers[index];

                if (!peer->welcomed)
                        continue;
                if (peer->active)
                        return POUDLAND_P0_SERVER_CONTINUE;
        }
        if (server->served_client)
                return POUDLAND_P0_SERVER_STOP_CLEAN;
        return startup_expired
                   ? POUDLAND_P0_SERVER_STOP_STARTUP_TIMEOUT
                   : POUDLAND_P0_SERVER_CONTINUE;
}

static void apply_damage(struct poudland_p0_server *server,
                         const struct poudland_p0_protocol_result *result)
{
        uint_32 index;

        if (!server->damage)
                return;
        for (index = 0; index < result->damage_count; ++index)
                server->damage(server, result->damages[index]);
}

static void cleanup_protocol_peer(struct poudland_p0_server *server,
                                  frog_pkg_peer_id peer_id)
{
        struct poudland_p0_protocol_result result;

        poudland_p0_protocol_handle_disconnect(
            &server->protocol, peer_id, &result);
        apply_damage(server, &result);
}

static void tombstone_peer(struct poudland_p0_server *server,
                           struct poudland_p0_server_peer *peer)
{
        /* packagefs has no server-side close-one-peer operation.  Once a
         * reliable reply cannot be retained, tear down application state and
         * ignore this generation until its DISCONNECT record arrives. */
        cleanup_protocol_peer(server, peer->peer_id);
        peer->reply_head = 0;
        peer->reply_count = 0;
        peer->closing = false;
        peer->tombstone = true;
}

static void store_pending(struct poudland_p0_server_reply *pending,
                          const void *data, uint_32 size,
                          bool replaceable, uint_32 type,
                          uint_32 window_id)
{
        clear_bytes(pending, sizeof(*pending));
        pending->size = size;
        pending->replaceable = replaceable;
        pending->type = type;
        pending->window_id = window_id;
        copy_bytes(pending->data, data, size);
}

static void remove_pending(struct poudland_p0_server_peer *peer,
                           uint_32 logical_index)
{
        uint_32 index;

        for (index = logical_index; index + 1U < peer->reply_count; ++index) {
                uint_32 destination = (peer->reply_head + index) %
                                      POUDLAND_P0_REPLY_QUEUE_MAX;
                uint_32 source = (peer->reply_head + index + 1U) %
                                 POUDLAND_P0_REPLY_QUEUE_MAX;

                peer->replies[destination] = peer->replies[source];
        }
        if (peer->reply_count != 0) {
                uint_32 tail = (peer->reply_head + peer->reply_count - 1U) %
                               POUDLAND_P0_REPLY_QUEUE_MAX;

                clear_bytes(&peer->replies[tail],
                            sizeof(peer->replies[tail]));
                peer->reply_count--;
        }
}

static bool enqueue_outbound(struct poudland_p0_server_peer *peer,
                             const void *data, uint_32 size,
                             bool replaceable, uint_32 type,
                             uint_32 window_id)
{
        uint_32 tail;
        struct poudland_p0_server_reply *pending;

        if (size > POUDLAND_V1_P0_MESSAGE_MAX)
                return false;
        if (replaceable) {
                uint_32 logical_index;

                for (logical_index = 0;
                     logical_index < peer->reply_count;
                     ++logical_index) {
                        uint_32 index = (peer->reply_head + logical_index) %
                                       POUDLAND_P0_REPLY_QUEUE_MAX;
                        struct poudland_p0_server_reply *old =
                            &peer->replies[index];
                        uint_32 later;
                        bool reliable_barrier = false;

                        if (!old->replaceable || old->type != type ||
                            old->window_id != window_id)
                                continue;
                        for (later = logical_index + 1U;
                             later < peer->reply_count; ++later) {
                                uint_32 later_index =
                                    (peer->reply_head + later) %
                                    POUDLAND_P0_REPLY_QUEUE_MAX;

                                if (!peer->replies[later_index].replaceable) {
                                        reliable_barrier = true;
                                        break;
                                }
                        }
                        if (!reliable_barrier) {
                                store_pending(old, data, size, true,
                                              type, window_id);
                                return true;
                        }
                        /* The newer state cannot be delivered ahead of a
                         * reliable transition.  Drop the stale slot and
                         * append the replacement after that barrier. */
                        remove_pending(peer, logical_index);
                        break;
                }
        }
        if (peer->reply_count >= POUDLAND_P0_REPLY_QUEUE_MAX)
                return false;
        tail = (peer->reply_head + peer->reply_count) %
               POUDLAND_P0_REPLY_QUEUE_MAX;
        pending = &peer->replies[tail];
        store_pending(pending, data, size, replaceable, type, window_id);
        peer->reply_count++;
        return true;
}

static bool enqueue_reply(struct poudland_p0_server_peer *peer,
                          const void *reply, uint_32 reply_size)
{
        const struct poudland_v1_header *header = reply;

        return enqueue_outbound(peer, reply, reply_size, false,
                                header->type, 0);
}

static bool flush_peer(struct poudland_p0_server *server,
                       struct poudland_p0_server_peer *peer)
{
        while (peer->reply_count != 0) {
                struct poudland_p0_server_reply *pending =
                    &peer->replies[peer->reply_head];
                int_32 status = server->send(
                    server, peer->peer_id, pending->data, pending->size);

                if (status == -EAGAIN)
                        return true;
                if (status != 0) {
                        tombstone_peer(server, peer);
                        return true;
                }
                record_delivered_reply(server, peer, pending->data,
                                       pending->size);
                clear_bytes(pending, sizeof(*pending));
                peer->reply_head = (peer->reply_head + 1U) %
                                   POUDLAND_P0_REPLY_QUEUE_MAX;
                peer->reply_count--;
        }
        peer->reply_head = 0;
        if (peer->closing)
                tombstone_peer(server, peer);
        return true;
}

static void begin_closing(struct poudland_p0_server *server,
                          struct poudland_p0_server_peer *peer,
                          const void *reply, uint_32 reply_size)
{
        int_32 status;

        peer->closing = true;
        if (peer->reply_count != 0) {
                (void) enqueue_reply(peer, reply, reply_size);
                return;
        }
        status = server->send(server, peer->peer_id, reply, reply_size);
        if (status == -EAGAIN) {
                (void) enqueue_reply(peer, reply, reply_size);
                return;
        }
        tombstone_peer(server, peer);
}

static bool deliver_reply(struct poudland_p0_server *server,
                          struct poudland_p0_server_peer *peer,
                          const void *reply, uint_32 reply_size)
{
        int_32 status;

        if (peer->reply_count != 0)
                return enqueue_reply(peer, reply, reply_size);
        status = server->send(server, peer->peer_id, reply, reply_size);
        if (status == 0) {
                record_delivered_reply(server, peer, reply, reply_size);
                return true;
        }
        if (status == -EAGAIN)
                return enqueue_reply(peer, reply, reply_size);
        tombstone_peer(server, peer);
        return true;
}

static bool event_delivery_class(const void *data, uint_32 size,
                                 bool *replaceable,
                                 uint_32 *window_id)
{
        const struct poudland_v1_header *header = data;
        const uint_8 *payload = data;

        if (size < POUDLAND_V1_HEADER_SIZE ||
            size != POUDLAND_V1_HEADER_SIZE + header->payload_size)
                return false;
        *replaceable = false;
        *window_id = 0;
        payload += POUDLAND_V1_HEADER_SIZE;
        if (header->type == POUDLAND_V1_MSG_WINDOW_CONFIGURE) {
                const struct poudland_v1_window_configure *configure;

                if (header->payload_size != sizeof(*configure))
                        return false;
                configure = (const struct poudland_v1_window_configure *)
                    payload;
                *replaceable = true;
                *window_id = configure->window_id;
                return true;
        }
        if (header->type == POUDLAND_V1_MSG_POINTER_EVENT) {
                const struct poudland_v1_pointer_event *pointer;

                if (header->payload_size != sizeof(*pointer))
                        return false;
                pointer = (const struct poudland_v1_pointer_event *) payload;
                *replaceable =
                    pointer->type == POUDLAND_V1_POINTER_MOVE ||
                    pointer->type == POUDLAND_V1_POINTER_DRAG;
                *window_id = pointer->window_id;
                return true;
        }
        if (header->type == POUDLAND_V1_MSG_KEY_EVENT &&
            header->payload_size == sizeof(struct poudland_v1_key_event))
                return true;
        return false;
}

static bool deliver_event(struct poudland_p0_server *server,
                          struct poudland_p0_server_peer *peer,
                          const void *data, uint_32 size)
{
        const struct poudland_v1_header *header = data;
        bool replaceable;
        uint_32 window_id;
        int_32 status;

        if (!event_delivery_class(data, size, &replaceable, &window_id))
                return false;
        if (peer->reply_count != 0) {
                if (!enqueue_outbound(peer, data, size, replaceable,
                                      header->type, window_id))
                        tombstone_peer(server, peer);
                return true;
        }
        status = server->send(server, peer->peer_id, data, size);
        if (status == 0)
                return true;
        if (status == -EAGAIN) {
                if (!enqueue_outbound(peer, data, size, replaceable,
                                      header->type, window_id))
                        tombstone_peer(server, peer);
                return true;
        }
        tombstone_peer(server, peer);
        return true;
}

static bool deliver_events(
    struct poudland_p0_server *server,
    const struct poudland_p0_protocol_result *result)
{
        uint_32 index;

        for (index = 0; index < result->event_count; ++index) {
                const struct poudland_p0_protocol_event *event =
                    &result->events[index];
                struct poudland_p0_server_peer *peer =
                    find_peer(server, event->peer_id);

                if (!peer || peer->tombstone || peer->closing)
                        continue;
                if (!deliver_event(server, peer, event->data, event->size)) {
                        tombstone_peer(server, peer);
                        return false;
                }
        }
        return true;
}

void poudland_p0_server_state_init(
    struct poudland_p0_server *server, uint_32 display_width,
    uint_32 display_height, poudland_p0_server_send_fn send,
    poudland_p0_server_damage_fn damage, void *transport_context)
{
        if (!server)
                return;
        clear_bytes(server, sizeof(*server));
        server->fd = -1;
        server->send = send;
        server->damage = damage;
        server->transport_context = transport_context;
        poudland_p0_protocol_init(&server->protocol,
                                  display_width, display_height);
}

bool poudland_p0_server_handle_record(
    struct poudland_p0_server *server,
    const struct frog_pkg_message *message)
{
        struct poudland_p0_server_peer *peer;

        if (!server || !message || message->peer_id == 0 || !server->send)
                return false;
        peer = find_peer(server, message->peer_id);
        if (message->event == FROG_PKG_DISCONNECT) {
                cleanup_protocol_peer(server, message->peer_id);
                if (peer)
                        clear_bytes(peer, sizeof(*peer));
                return true;
        }
        if (message->event == FROG_PKG_WRITABLE)
                return !peer || peer->tombstone || flush_peer(server, peer);
        if (message->event != FROG_PKG_DATA)
                return false;
        if (!peer)
                peer = allocate_peer(server, message->peer_id);
        if (!peer)
                return true;
        if (peer->tombstone || peer->closing)
                return true;
        if (peer->reply_count == POUDLAND_P0_REPLY_QUEUE_MAX) {
                tombstone_peer(server, peer);
                return true;
        }

        struct poudland_p0_protocol_result result;
        int_32 status = poudland_p0_protocol_handle_data(
            &server->protocol, message->peer_id, message->payload,
            message->payload_size, &result);

        if (status != 0 || result.reply_size == 0) {
                tombstone_peer(server, peer);
                return true;
        }
        apply_damage(server, &result);
        if (result.terminate_session) {
                begin_closing(server, peer, result.reply,
                              result.reply_size);
                return true;
        }
        if (!deliver_reply(server, peer, result.reply, result.reply_size)) {
                tombstone_peer(server, peer);
                return true;
        }
        return true;
}

bool poudland_p0_server_handle_pointer(
    struct poudland_p0_server *server, int_32 screen_x,
    int_32 screen_y, uint_32 buttons)
{
        struct poudland_p0_protocol_result result;

        if (!server || !server->send ||
            poudland_p0_protocol_handle_pointer(
                &server->protocol, screen_x, screen_y,
                buttons, &result) != 0)
                return false;
        apply_damage(server, &result);
        return deliver_events(server, &result);
}

bool poudland_p0_server_handle_key(
    struct poudland_p0_server *server, uint_8 key)
{
        struct poudland_p0_protocol_result result;

        if (!server || !server->send ||
            poudland_p0_protocol_handle_key(
                &server->protocol, key, &result) != 0)
                return false;
        apply_damage(server, &result);
        return deliver_events(server, &result);
}

#ifndef POUDLAND_P0_SERVER_HOST_TEST
#define POUDLAND_P0_STARTUP_TIMEOUT_SECONDS 5

static bool timespec_at_or_after(const struct timespec *left,
                                 const struct timespec *right)
{
        return left->tv_sec > right->tv_sec ||
               (left->tv_sec == right->tv_sec &&
                left->tv_nsec >= right->tv_nsec);
}

static int_32 packagefs_send(struct poudland_p0_server *server,
                             frog_pkg_peer_id peer_id,
                             const void *payload, uint_32 payload_size)
{
        int_32 status = frog_pkg_server_send(
            server->fd, peer_id, payload, payload_size);

        if (status == (int_32) (FROG_PKG_HEADER_SIZE + payload_size))
                return 0;
        return status;
}

static void scene_damage(struct poudland_p0_server *server,
                         struct poudland_p0_rect rect)
{
        struct poudland_p0_scene *scene = server->transport_context;

        poudland_p0_damage(&scene->display, rect);
}

int_32 poudland_p0_server_open(struct poudland_p0_server *server,
                               struct poudland_p0_scene *scene)
{
        int_32 fd;

        if (!server || !scene)
                return -EINVAL;
        poudland_p0_server_state_init(
            server, scene->display.info.width, scene->display.info.height,
            packagefs_send, scene_damage, scene);
        fd = frog_pkg_bind("compositor", true);
        if (fd < 0)
                return fd;
        server->fd = fd;
        scene->client_protocol = &server->protocol;
        return 0;
}

bool poudland_p0_server_run(struct poudland_p0_server *server,
                            struct poudland_p0_scene *scene)
{
        struct pollfd descriptors[3];
        struct timespec startup_deadline;
        const uint_16 errors = POLLERR | POLLHUP | POLLNVAL;

        if (!server || !scene || server->fd < 0 ||
            scene->keyboard_fd < 0 || scene->mouse_fd < 0)
                return false;
        if (clock_gettime(CLOCK_MONOTONIC, &startup_deadline) != 0)
                return false;
        startup_deadline.tv_sec += POUDLAND_P0_STARTUP_TIMEOUT_SECONDS;
        for (;;) {
                enum poudland_p0_server_lifecycle lifecycle;
                struct timespec now;
                int_32 status;

                descriptors[0].fd = server->fd;
                descriptors[0].events = POLLIN;
                descriptors[0].revents = 0;
                descriptors[1].fd = scene->keyboard_fd;
                descriptors[1].events = POLLIN;
                descriptors[1].revents = 0;
                descriptors[2].fd = scene->mouse_fd;
                descriptors[2].events = POLLIN;
                descriptors[2].revents = 0;
                status = wait2(descriptors, 3, 1000);
                if (status < 0 || (descriptors[0].revents & errors) != 0 ||
                    (descriptors[1].revents & errors) != 0 ||
                    (descriptors[2].revents & errors) != 0)
                        return false;
                if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
                        return false;
                lifecycle = poudland_p0_server_lifecycle(
                    server, timespec_at_or_after(&now, &startup_deadline));
                if (lifecycle == POUDLAND_P0_SERVER_STOP_CLEAN)
                        return true;
                if (lifecycle == POUDLAND_P0_SERVER_STOP_STARTUP_TIMEOUT)
                        return false;
                if (status == 0)
                        continue;
                if ((descriptors[0].revents & POLLIN) != 0) {
                        for (;;) {
                                struct frog_pkg_message message;

                                status = frog_pkg_server_receive(
                                    server->fd, &message);
                                if (status == -EAGAIN)
                                        break;
                                if (status <= 0 ||
                                    !poudland_p0_server_handle_record(
                                        server, &message))
                                        return false;
                        }
                        /* Drain the shared packagefs queue before deciding
                         * that the last welcomed client is gone.  A HELLO
                         * from another peer may follow that disconnect. */
                        lifecycle = poudland_p0_server_lifecycle(
                            server, false);
                        if (lifecycle == POUDLAND_P0_SERVER_STOP_CLEAN)
                                return true;
                }
                if ((descriptors[2].revents & POLLIN) != 0) {
                        for (;;) {
                                mouse_device_packet_t packet;
                                struct poudland_p0_rect old_cursor;
                                int_64 next_x;
                                int_64 next_y;
                                int_32 maximum_x;
                                int_32 maximum_y;

                                status = read(scene->mouse_fd, &packet,
                                              sizeof(packet));
                                if (status == -EAGAIN)
                                        break;
                                if (status != sizeof(packet) ||
                                    packet.magic != MOUSE_MAGIC)
                                        return false;
                                old_cursor = (struct poudland_p0_rect) {
                                    .x = scene->cursor_x,
                                    .y = scene->cursor_y,
                                    .width = (int_32) scene->cursor.width,
                                    .height = (int_32) scene->cursor.height,
                                };
                                maximum_x = scene->display.info.width == 0
                                                ? 0
                                                : (int_32)
                                                      scene->display.info.width -
                                                      1;
                                maximum_y = scene->display.info.height == 0
                                                ? 0
                                                : (int_32)
                                                      scene->display.info.height -
                                                      1;
                                next_x = (int_64) scene->cursor_x +
                                         packet.x_difference;
                                next_y = (int_64) scene->cursor_y -
                                         packet.y_difference;
                                if (next_x < 0)
                                        next_x = 0;
                                if (next_x > maximum_x)
                                        next_x = maximum_x;
                                if (next_y < 0)
                                        next_y = 0;
                                if (next_y > maximum_y)
                                        next_y = maximum_y;
                                scene->cursor_x = (int_32) next_x;
                                scene->cursor_y = (int_32) next_y;
                                if (scene->cursor_x != old_cursor.x ||
                                    scene->cursor_y != old_cursor.y) {
                                        poudland_p0_damage(&scene->display,
                                                           old_cursor);
                                        poudland_p0_damage(
                                            &scene->display,
                                            (struct poudland_p0_rect) {
                                                .x = scene->cursor_x,
                                                .y = scene->cursor_y,
                                                .width = (int_32)
                                                    scene->cursor.width,
                                                .height = (int_32)
                                                    scene->cursor.height,
                                            });
                                }
                                scene->mouse_buttons = packet.buttons;
                                if (!poudland_p0_server_handle_pointer(
                                        server, scene->cursor_x,
                                        scene->cursor_y, packet.buttons))
                                        return false;
                        }
                }
                /* Mouse transitions establish focus before keyboard bytes
                 * from the same readiness snapshot are delivered. */
                if ((descriptors[1].revents & POLLIN) != 0) {
                        for (;;) {
                                uint_8 key;

                                status = read(scene->keyboard_fd, &key,
                                              sizeof(key));
                                if (status == -EAGAIN)
                                        break;
                                if (status != sizeof(key) ||
                                    !poudland_p0_server_handle_key(
                                        server, key))
                                        return false;
                        }
                }
                if (scene->display.damaged &&
                    !poudland_p0_scene_present(scene))
                        return false;
        }
}

void poudland_p0_server_close(struct poudland_p0_server *server)
{
        if (!server)
                return;
        if (server->fd >= 0)
                (void) close(server->fd);
        server->fd = -1;
}
#endif
