#ifndef _FROG_APPS_PACKAGEFS_H
#define _FROG_APPS_PACKAGEFS_H

#include <frog/types.h>
#include <uapi/frog/packagefs.h>

typedef uint_32 frog_pkg_peer_id;

struct frog_pkg_message {
        frog_pkg_peer_id peer_id;
        uint_32 event;
        uint_32 payload_size;
        uint_8 payload[FROG_PKG_PAYLOAD_MAX];
};

typedef char frog_pkg_message_must_match_record_size[
    sizeof(struct frog_pkg_message) == FROG_PKG_RECORD_MAX ? 1 : -1];
typedef char frog_pkg_message_must_match_record_layout[
    __builtin_offsetof(struct frog_pkg_message, peer_id) == 0U &&
            __builtin_offsetof(struct frog_pkg_message, event) == 4U &&
            __builtin_offsetof(struct frog_pkg_message, payload_size) == 8U &&
            __builtin_offsetof(struct frog_pkg_message, payload) ==
                FROG_PKG_HEADER_SIZE
        ? 1
        : -1];

enum frog_pkg_delivery_class {
        FROG_PKG_DELIVERED = 1,
        FROG_PKG_WOULD_BLOCK,
        FROG_PKG_DISCONNECTED,
        FROG_PKG_DELIVERY_ERROR,
};

struct frog_pkg_delivery_result {
        frog_pkg_peer_id peer_id;
        int_32 raw_status;
        enum frog_pkg_delivery_class classification;
};

/* Bind/connect return an endpoint fd or the original negative errno. */
int_32 frog_pkg_bind(const char *service, bool nonblock);
int_32 frog_pkg_connect(const char *service, bool nonblock);

/* Send/receive return the exact 12 + payload wire length, zero for EOF,
 * or the original negative errno. A nonzero payload requires a non-NULL
 * payload pointer. */
int_32 frog_pkg_client_send(int_32 fd, const void *payload,
                            uint_32 payload_size);
int_32 frog_pkg_server_send(int_32 fd, frog_pkg_peer_id peer_id,
                            const void *payload, uint_32 payload_size);
int_32 frog_pkg_client_receive(int_32 fd, struct frog_pkg_message *message);
int_32 frog_pkg_server_receive(int_32 fd, struct frog_pkg_message *message);

/* Broadcast snapshots explicit peers and attempts every entry. The fd must
 * come from frog_pkg_bind(..., true); otherwise an individual directed send
 * may block and the expansion is not nonblocking. It returns zero after
 * expansion; each entry retains its raw send result and class. Common
 * argument errors are returned before any send is attempted. */
int_32 frog_pkg_server_broadcast(
    int_32 fd, const frog_pkg_peer_id *peer_snapshot, uint_32 peer_count,
    const void *payload, uint_32 payload_size,
    struct frog_pkg_delivery_result *results);

#endif
