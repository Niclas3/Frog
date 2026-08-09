#include "server.h"

#include <frog/errno.h>

#ifndef POUDLAND_P0_SERVER_HOST_TEST
#include "poudland_p0.h"

#include <frog/poll.h>
#include <frog/syscall.h>
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

static bool enqueue_reply(struct poudland_p0_server_peer *peer,
                          const void *reply, uint_32 reply_size)
{
        uint_32 tail;
        struct poudland_p0_server_reply *pending;

        if (peer->reply_count >= POUDLAND_P0_REPLY_QUEUE_MAX ||
            reply_size > POUDLAND_V1_P0_MESSAGE_MAX)
                return false;
        tail = (peer->reply_head + peer->reply_count) %
               POUDLAND_P0_REPLY_QUEUE_MAX;
        pending = &peer->replies[tail];
        pending->size = reply_size;
        copy_bytes(pending->data, reply, reply_size);
        peer->reply_count++;
        return true;
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
        if (status == 0)
                return true;
        if (status == -EAGAIN)
                return enqueue_reply(peer, reply, reply_size);
        tombstone_peer(server, peer);
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

#ifndef POUDLAND_P0_SERVER_HOST_TEST
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
        struct pollfd descriptor;

        if (!server || !scene || server->fd < 0)
                return false;
        for (;;) {
                int_32 status;

                descriptor.fd = server->fd;
                descriptor.events = POLLIN;
                descriptor.revents = 0;
                status = wait2(&descriptor, 1, 1000);
                if (status < 0 ||
                    (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
                        return false;
                if (status == 0)
                        continue;
                for (;;) {
                        struct frog_pkg_message message;

                        status = frog_pkg_server_receive(server->fd, &message);
                        if (status == -EAGAIN)
                                break;
                        if (status <= 0 ||
                            !poudland_p0_server_handle_record(server, &message))
                                return false;
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
