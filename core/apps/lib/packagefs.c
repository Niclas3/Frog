#include <frog/errno.h>
#include <frog/fcntl.h>
#include <frog/packagefs.h>
#include <frog/syscall.h>

#define FROG_PKG_PATH_PREFIX "/dev/pkg/"
#define FROG_PKG_PATH_PREFIX_LENGTH 9U

static int frog_pkg_make_path(const char *service,
                              char path[FROG_PKG_PATH_PREFIX_LENGTH +
                                        FROG_PKG_NAME_MAX + 1U])
{
        uint_32 length = 0;

        if (service == NULL)
                return -EINVAL;
        while (service[length] != '\0') {
                if (length == FROG_PKG_NAME_MAX)
                        return -ENAMETOOLONG;
                if (service[length] == '/')
                        return -EINVAL;
                length++;
        }
        if (length == 0 ||
            (length == 1 && service[0] == '.') ||
            (length == 2 && service[0] == '.' && service[1] == '.'))
                return -EINVAL;
        for (uint_32 index = 0; index < FROG_PKG_PATH_PREFIX_LENGTH; index++)
                path[index] = FROG_PKG_PATH_PREFIX[index];
        for (uint_32 index = 0; index <= length; index++)
                path[FROG_PKG_PATH_PREFIX_LENGTH + index] = service[index];
        return 0;
}

int_32 frog_pkg_bind(const char *service, bool nonblock)
{
        char path[FROG_PKG_PATH_PREFIX_LENGTH + FROG_PKG_NAME_MAX + 1U];
        int result = frog_pkg_make_path(service, path);
        uint_32 flags = O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC;

        if (result != 0)
                return result;
        if (nonblock)
                flags |= O_NONBLOCK;
        return open(path, flags);
}

int_32 frog_pkg_connect(const char *service, bool nonblock)
{
        char path[FROG_PKG_PATH_PREFIX_LENGTH + FROG_PKG_NAME_MAX + 1U];
        int result = frog_pkg_make_path(service, path);
        uint_32 flags = O_RDWR | O_CLOEXEC;

        if (result != 0)
                return result;
        if (nonblock)
                flags |= O_NONBLOCK;
        return open(path, flags);
}

static int_32 frog_pkg_send_data(int_32 fd, frog_pkg_peer_id peer_id,
                                 const void *payload, uint_32 payload_size)
{
        uint_8 storage[FROG_PKG_RECORD_MAX];
        struct frog_pkg_record *record = (struct frog_pkg_record *) storage;
        const uint_8 *bytes = payload;

        if (payload_size > FROG_PKG_PAYLOAD_MAX)
                return -EMSGSIZE;
        if (payload_size != 0 && payload == NULL)
                return -EINVAL;
        record->peer_id = peer_id;
        record->event = FROG_PKG_DATA;
        record->payload_size = payload_size;
        for (uint_32 index = 0; index < payload_size; index++)
                record->payload[index] = bytes[index];
        return write(fd, record, FROG_PKG_HEADER_SIZE + payload_size);
}

int_32 frog_pkg_client_send(int_32 fd, const void *payload,
                            uint_32 payload_size)
{
        return frog_pkg_send_data(fd, 0, payload, payload_size);
}

int_32 frog_pkg_server_send(int_32 fd, frog_pkg_peer_id peer_id,
                            const void *payload, uint_32 payload_size)
{
        if (peer_id == 0)
                return -EINVAL;
        return frog_pkg_send_data(fd, peer_id, payload, payload_size);
}

static int_32 frog_pkg_receive(int_32 fd, struct frog_pkg_message *message,
                               bool server_role)
{
        uint_8 storage[FROG_PKG_RECORD_MAX];
        struct frog_pkg_record *record = (struct frog_pkg_record *) storage;
        int_32 result;

        if (message == NULL)
                return -EINVAL;
        result = read(fd, record, sizeof(storage));
        if (result <= 0)
                return result;
        if (result < (int_32) FROG_PKG_HEADER_SIZE ||
            result > (int_32) FROG_PKG_RECORD_MAX ||
            record->payload_size > FROG_PKG_PAYLOAD_MAX ||
            result != (int_32) (FROG_PKG_HEADER_SIZE +
                                record->payload_size))
                return -EPROTO;
        if (record->event != FROG_PKG_DATA &&
            record->event != FROG_PKG_DISCONNECT &&
            record->event != FROG_PKG_WRITABLE)
                return -EPROTO;
        if (record->event != FROG_PKG_DATA && record->payload_size != 0)
                return -EPROTO;
        if ((server_role && record->peer_id == 0) ||
            (!server_role && (record->peer_id != 0 ||
                              record->event != FROG_PKG_DATA)))
                return -EPROTO;

        message->peer_id = record->peer_id;
        message->event = record->event;
        message->payload_size = record->payload_size;
        for (uint_32 index = 0; index < record->payload_size; index++)
                message->payload[index] = record->payload[index];
        return result;
}

int_32 frog_pkg_client_receive(int_32 fd, struct frog_pkg_message *message)
{
        return frog_pkg_receive(fd, message, false);
}

int_32 frog_pkg_server_receive(int_32 fd, struct frog_pkg_message *message)
{
        return frog_pkg_receive(fd, message, true);
}

int_32 frog_pkg_server_broadcast(
    int_32 fd, const frog_pkg_peer_id *peer_snapshot, uint_32 peer_count,
    const void *payload, uint_32 payload_size,
    struct frog_pkg_delivery_result *results)
{
        if (payload_size > FROG_PKG_PAYLOAD_MAX)
                return -EMSGSIZE;
        if (payload_size != 0 && payload == NULL)
                return -EINVAL;
        if (peer_count > FROG_PKG_CLIENT_MAX ||
            (peer_count != 0 &&
             (peer_snapshot == NULL || results == NULL)))
                return -EINVAL;

        for (uint_32 index = 0; index < peer_count; index++) {
                int_32 status;

                results[index].peer_id = peer_snapshot[index];
                if (peer_snapshot[index] == 0)
                        status = -EINVAL;
                else
                        status = frog_pkg_server_send(
                            fd, peer_snapshot[index], payload, payload_size);
                results[index].raw_status = status;
                if (status ==
                    (int_32) (FROG_PKG_HEADER_SIZE + payload_size))
                        results[index].classification = FROG_PKG_DELIVERED;
                else if (status == -EAGAIN)
                        results[index].classification =
                            FROG_PKG_WOULD_BLOCK;
                else if (status == -ENOENT)
                        results[index].classification =
                            FROG_PKG_DISCONNECTED;
                else
                        results[index].classification =
                            FROG_PKG_DELIVERY_ERROR;
        }
        return 0;
}
