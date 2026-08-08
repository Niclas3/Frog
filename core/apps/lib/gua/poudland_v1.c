#include <frog/errno.h>
#include <frog/packagefs.h>
#include <frog/poll.h>
#include <frog/syscall.h>
#include <frog/time.h>
#include <gua/poudland_v1.h>

#define POUDLAND_V1_CONNECT_RETRY_MS 10
#define POUDLAND_V1_INT32_MAX        0x7fffffff

struct poudland_v1_deadline {
        struct timespec at;
};

enum poudland_v1_message_class {
        POUDLAND_V1_CLASS_REQUEST,
        POUDLAND_V1_CLASS_RESPONSE,
        POUDLAND_V1_CLASS_EVENT,
};

static void poudland_v1_copy_message(void *destination, const void *source)
{
        const struct poudland_v1_header *header = source;
        uint_32 size = POUDLAND_V1_HEADER_SIZE + header->payload_size;

        for (uint_32 index = 0; index < size; index++)
                ((uint_8 *) destination)[index] =
                    ((const uint_8 *) source)[index];
}

void poudland_v1_context_init(struct poudland_v1_context *context)
{
        if (context == NULL)
                return;
        context->fd = -1;
        context->version = 0;
        context->connected = 0;
        context->display_width = 0;
        context->display_height = 0;
        context->client_capabilities = 0;
        context->server_capabilities = 0;
        context->next_request_id = 1;
        context->inbox_count = 0;
        context->event_head = 0;
        context->event_count = 0;
        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++)
                context->pending[index].occupied = 0;
}

static int_32 poudland_v1_fail(struct poudland_v1_context *context,
                              int_32 status)
{
        int_32 fd = context->fd;

        poudland_v1_context_init(context);
        if (fd >= 0)
                (void) close(fd);
        return status;
}

static int_32 poudland_v1_deadline_init(struct poudland_v1_deadline *deadline,
                                       int_32 timeout_ms)
{
        int_32 status;

        if (timeout_ms < 0)
                return -EINVAL;
        status = clock_gettime(CLOCK_MONOTONIC, &deadline->at);
        if (status != 0)
                return status;
        deadline->at.tv_sec += timeout_ms / 1000;
        deadline->at.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (deadline->at.tv_nsec >= 1000000000) {
                deadline->at.tv_sec++;
                deadline->at.tv_nsec -= 1000000000;
        }
        return 0;
}

static int_32 poudland_v1_deadline_remaining(
    const struct poudland_v1_deadline *deadline, int_32 *remaining_ms)
{
        struct timespec now;
        time_t seconds;
        int_32 nanoseconds;
        uint_32 milliseconds;
        int_32 status = clock_gettime(CLOCK_MONOTONIC, &now);

        if (status != 0)
                return status;
        seconds = deadline->at.tv_sec - now.tv_sec;
        nanoseconds = deadline->at.tv_nsec - now.tv_nsec;
        if (nanoseconds < 0) {
                seconds--;
                nanoseconds += 1000000000;
        }
        if (seconds < 0 || (seconds == 0 && nanoseconds == 0)) {
                *remaining_ms = 0;
                return 0;
        }
        if (seconds > 2147483) {
                *remaining_ms = POUDLAND_V1_INT32_MAX;
                return 0;
        }
        milliseconds = (uint_32) seconds * 1000U;
        milliseconds += (uint_32) (nanoseconds + 999999) / 1000000U;
        if (milliseconds > POUDLAND_V1_INT32_MAX)
                milliseconds = POUDLAND_V1_INT32_MAX;
        *remaining_ms = (int_32) milliseconds;
        return 0;
}

static int_32 poudland_v1_expected_response(uint_32 request_type,
                                            uint_32 *response_type,
                                            uint_32 *payload_size)
{
        switch (request_type) {
        case POUDLAND_V1_MSG_HELLO:
                *response_type = POUDLAND_V1_MSG_WELCOME;
                *payload_size = sizeof(struct poudland_v1_hello);
                return 0;
        case POUDLAND_V1_MSG_WINDOW_NEW:
                *response_type = POUDLAND_V1_MSG_WINDOW_INIT;
                *payload_size = sizeof(struct poudland_v1_window_new);
                return 0;
        case POUDLAND_V1_MSG_WINDOW_CLOSE:
                *response_type = POUDLAND_V1_MSG_WINDOW_CLOSED;
                *payload_size = sizeof(struct poudland_v1_window_close);
                return 0;
        default:
                return -EINVAL;
        }
}

static struct poudland_v1_pending *poudland_v1_find_pending(
    struct poudland_v1_context *context, uint_32 request_id)
{
        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++) {
                if (context->pending[index].occupied &&
                    context->pending[index].request_id == request_id)
                        return &context->pending[index];
        }
        return NULL;
}

static struct poudland_v1_pending *poudland_v1_empty_pending(
    struct poudland_v1_context *context)
{
        for (uint_32 index = 0; index < POUDLAND_V1_PENDING_MAX; index++) {
                if (!context->pending[index].occupied)
                        return &context->pending[index];
        }
        return NULL;
}

static int_32 poudland_v1_choose_request_id(
    struct poudland_v1_context *context, uint_32 *request_id)
{
        uint_32 candidate = context->next_request_id;

        for (uint_32 attempt = 0; attempt <= POUDLAND_V1_PENDING_MAX;
             attempt++) {
                if (candidate == 0)
                        candidate = 1;
                if (poudland_v1_find_pending(context, candidate) == NULL) {
                        *request_id = candidate;
                        return 0;
                }
                candidate++;
        }
        return -ENOSPC;
}

int_32 poudland_v1_begin_request(struct poudland_v1_context *context,
                                uint_32 request_type, const void *payload,
                                uint_32 payload_size, uint_32 *request_id)
{
        struct poudland_v1_pending *pending;
        struct poudland_v1_message message;
        uint_32 expected_type;
        uint_32 expected_payload_size;
        uint_32 chosen_id;
        int_32 status;

        if (context == NULL || request_id == NULL)
                return -EINVAL;
        if (!context->connected || context->fd < 0)
                return -ENOTCONN;
        status = poudland_v1_expected_response(
            request_type, &expected_type, &expected_payload_size);
        if (status != 0)
                return status;
        if (payload_size != expected_payload_size ||
            (payload_size != 0 && payload == NULL) ||
            payload_size > POUDLAND_V1_P0_PAYLOAD_MAX)
                return -EINVAL;
        if (request_type != POUDLAND_V1_MSG_HELLO &&
            context->version != POUDLAND_V1_VERSION)
                return -EPROTONOSUPPORT;
        pending = poudland_v1_empty_pending(context);
        if (pending == NULL)
                return -ENOSPC;
        status = poudland_v1_choose_request_id(context, &chosen_id);
        if (status != 0)
                return status;

        message.header.magic = POUDLAND_V1_MAGIC;
        message.header.version = POUDLAND_V1_VERSION;
        message.header.header_size = POUDLAND_V1_HEADER_SIZE;
        message.header.type = request_type;
        message.header.request_id = chosen_id;
        message.header.payload_size = payload_size;
        for (uint_32 index = 0; index < payload_size; index++)
                message.payload[index] = ((const uint_8 *) payload)[index];
        status = frog_pkg_client_send(context->fd, &message,
                                      POUDLAND_V1_HEADER_SIZE + payload_size);
        if (status < 0)
                return status;
        if (status != (int_32) (FROG_PKG_HEADER_SIZE +
                                POUDLAND_V1_HEADER_SIZE + payload_size))
                return -EIO;

        pending->request_id = chosen_id;
        pending->request_type = request_type;
        pending->response_type = expected_type;
        pending->occupied = 1;
        context->next_request_id = chosen_id + 1U;
        *request_id = chosen_id;
        return 0;
}

static int_32 poudland_v1_payload_contract(
    uint_32 type, uint_32 *payload_size,
    enum poudland_v1_message_class *message_class)
{
        switch (type) {
        case POUDLAND_V1_MSG_HELLO:
                *payload_size = sizeof(struct poudland_v1_hello);
                *message_class = POUDLAND_V1_CLASS_REQUEST;
                return 0;
        case POUDLAND_V1_MSG_WINDOW_NEW:
                *payload_size = sizeof(struct poudland_v1_window_new);
                *message_class = POUDLAND_V1_CLASS_REQUEST;
                return 0;
        case POUDLAND_V1_MSG_WINDOW_CLOSE:
                *payload_size = sizeof(struct poudland_v1_window_close);
                *message_class = POUDLAND_V1_CLASS_REQUEST;
                return 0;
        case POUDLAND_V1_MSG_WELCOME:
                *payload_size = sizeof(struct poudland_v1_welcome);
                *message_class = POUDLAND_V1_CLASS_RESPONSE;
                return 0;
        case POUDLAND_V1_MSG_WINDOW_INIT:
                *payload_size = sizeof(struct poudland_v1_window_init);
                *message_class = POUDLAND_V1_CLASS_RESPONSE;
                return 0;
        case POUDLAND_V1_MSG_WINDOW_CLOSED:
                *payload_size = sizeof(struct poudland_v1_window_closed);
                *message_class = POUDLAND_V1_CLASS_RESPONSE;
                return 0;
        case POUDLAND_V1_MSG_ERROR:
                *payload_size = sizeof(struct poudland_v1_error);
                *message_class = POUDLAND_V1_CLASS_RESPONSE;
                return 0;
        case POUDLAND_V1_MSG_WINDOW_CONFIGURE:
                *payload_size = sizeof(struct poudland_v1_window_configure);
                *message_class = POUDLAND_V1_CLASS_EVENT;
                return 0;
        case POUDLAND_V1_MSG_POINTER_EVENT:
                *payload_size = sizeof(struct poudland_v1_pointer_event);
                *message_class = POUDLAND_V1_CLASS_EVENT;
                return 0;
        case POUDLAND_V1_MSG_KEY_EVENT:
                *payload_size = sizeof(struct poudland_v1_key_event);
                *message_class = POUDLAND_V1_CLASS_EVENT;
                return 0;
        default:
                return -EPROTO;
        }
}

static int_32 poudland_v1_decode(const struct frog_pkg_message *package,
                                struct poudland_v1_message *message,
                                enum poudland_v1_message_class *message_class)
{
        const struct poudland_v1_header *header;
        uint_32 expected_payload_size;
        int_32 status;

        if (package->payload_size < POUDLAND_V1_HEADER_SIZE ||
            package->payload_size > POUDLAND_V1_P0_MESSAGE_MAX)
                return -EPROTO;
        header = (const struct poudland_v1_header *) package->payload;
        if (header->magic != POUDLAND_V1_MAGIC ||
            header->version != POUDLAND_V1_VERSION ||
            header->header_size != POUDLAND_V1_HEADER_SIZE ||
            header->payload_size > POUDLAND_V1_P0_PAYLOAD_MAX ||
            package->payload_size !=
                POUDLAND_V1_HEADER_SIZE + header->payload_size)
                return -EPROTO;
        status = poudland_v1_payload_contract(
            header->type, &expected_payload_size, message_class);
        if (status != 0 || header->payload_size != expected_payload_size)
                return -EPROTO;
        if ((*message_class == POUDLAND_V1_CLASS_EVENT &&
             header->request_id != 0) ||
            (*message_class != POUDLAND_V1_CLASS_EVENT &&
             header->request_id == 0))
                return -EPROTO;

        message->header.magic = header->magic;
        message->header.version = header->version;
        message->header.header_size = header->header_size;
        message->header.type = header->type;
        message->header.request_id = header->request_id;
        message->header.payload_size = header->payload_size;
        for (uint_32 index = 0; index < header->payload_size; index++)
                message->payload[index] =
                    package->payload[POUDLAND_V1_HEADER_SIZE + index];
        return 0;
}

static int_32 poudland_v1_validate_response(
    const struct poudland_v1_pending *pending,
    const struct poudland_v1_message *message)
{
        if (message->header.type == POUDLAND_V1_MSG_ERROR) {
                const struct poudland_v1_error *error =
                    (const struct poudland_v1_error *) message->payload;

                if (error->status >= 0 ||
                    error->failed_type != pending->request_type)
                        return -EPROTO;
                return 0;
        }
        return message->header.type == pending->response_type ? 0 : -EPROTO;
}

static int_32 poudland_v1_read_message(
    struct poudland_v1_context *context,
    const struct poudland_v1_deadline *deadline,
    struct poudland_v1_message *message,
    enum poudland_v1_message_class *message_class)
{
        struct frog_pkg_message package;
        struct pollfd descriptor;
        int_32 remaining_ms;
        int_32 status;

        for (;;) {
                status = poudland_v1_deadline_remaining(deadline,
                                                        &remaining_ms);
                if (status != 0)
                        return poudland_v1_fail(context, status);
                descriptor.fd = context->fd;
                descriptor.events = POLLIN;
                descriptor.revents = 0;
                status = wait2(&descriptor, 1, remaining_ms);
                if (status == 0)
                        return -ETIMEDOUT;
                if (status < 0)
                        return poudland_v1_fail(context, status);
                if (descriptor.revents & POLLIN) {
                        status = frog_pkg_client_receive(context->fd,
                                                         &package);
                        if (status > 0) {
                                status = poudland_v1_decode(
                                    &package, message, message_class);
                                if (status != 0)
                                        return poudland_v1_fail(context,
                                                                status);
                                return 0;
                        }
                        if (status == 0)
                                return poudland_v1_fail(context,
                                                       -ECONNRESET);
                        if (status == -EAGAIN)
                                continue;
                        return poudland_v1_fail(context, status);
                }
                if (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL))
                        return poudland_v1_fail(context, -ECONNRESET);
        }
}

static int_32 poudland_v1_take_inbox(struct poudland_v1_context *context,
                                    uint_32 request_id,
                                    struct poudland_v1_message *message)
{
        for (uint_32 index = 0; index < context->inbox_count; index++) {
                if (context->inbox[index].header.request_id == request_id) {
                        poudland_v1_copy_message(
                            message, &context->inbox[index]);
                        context->inbox_count--;
                        if (index != context->inbox_count)
                                poudland_v1_copy_message(
                                    &context->inbox[index],
                                    &context->inbox[context->inbox_count]);
                        return 1;
                }
        }
        return 0;
}

static int_32 poudland_v1_store_inbox(
    struct poudland_v1_context *context,
    const struct poudland_v1_message *message)
{
        if (context->inbox_count == POUDLAND_V1_INBOX_CAPACITY)
                return poudland_v1_fail(context, -ENOBUFS);
        for (uint_32 index = 0; index < context->inbox_count; index++) {
                if (context->inbox[index].header.request_id ==
                    message->header.request_id)
                        return poudland_v1_fail(context, -EPROTO);
        }
        poudland_v1_copy_message(&context->inbox[context->inbox_count],
                                 message);
        context->inbox_count++;
        return 0;
}

static int_32 poudland_v1_store_event(
    struct poudland_v1_context *context,
    const struct poudland_v1_message *message)
{
        uint_32 index;

        if (context->event_count == POUDLAND_V1_EVENT_CAPACITY)
                return poudland_v1_fail(context, -ENOBUFS);
        index = (context->event_head + context->event_count) %
                POUDLAND_V1_EVENT_CAPACITY;
        poudland_v1_copy_message(&context->events[index], message);
        context->event_count++;
        return 0;
}

static int_32 poudland_v1_consume_response(
    struct poudland_v1_pending *pending,
    const struct poudland_v1_message *message,
    struct poudland_v1_message *response)
{
        int_32 status = 0;

        poudland_v1_copy_message(response, message);
        if (message->header.type == POUDLAND_V1_MSG_ERROR)
                status = ((const struct poudland_v1_error *)
                              message->payload)->status;
        pending->occupied = 0;
        return status;
}

static int_32 poudland_v1_wait_handshake_until(
    struct poudland_v1_context *context, uint_32 request_id,
    const struct poudland_v1_deadline *deadline,
    struct poudland_v1_message *response)
{
        struct poudland_v1_pending *pending =
            poudland_v1_find_pending(context, request_id);
        struct poudland_v1_message message;
        enum poudland_v1_message_class message_class;
        int_32 status;

        if (pending == NULL)
                return -EINVAL;
        status = poudland_v1_read_message(context, deadline, &message,
                                          &message_class);
        if (status != 0)
                return status;
        if (message_class != POUDLAND_V1_CLASS_RESPONSE ||
            message.header.request_id != request_id ||
            poudland_v1_validate_response(pending, &message) != 0)
                return poudland_v1_fail(context, -EPROTO);
        return poudland_v1_consume_response(pending, &message, response);
}

static int_32 poudland_v1_wait_until(
    struct poudland_v1_context *context, uint_32 request_id,
    const struct poudland_v1_deadline *deadline,
    struct poudland_v1_message *output)
{
        struct poudland_v1_pending *pending = NULL;
        struct poudland_v1_message message;
        enum poudland_v1_message_class message_class;
        int_32 status;

        if (request_id == 0) {
                if (context->event_count != 0) {
                        poudland_v1_copy_message(
                            output, &context->events[context->event_head]);
                        context->event_head =
                            (context->event_head + 1U) %
                            POUDLAND_V1_EVENT_CAPACITY;
                        context->event_count--;
                        return 0;
                }
        } else {
                pending = poudland_v1_find_pending(context, request_id);
                if (pending == NULL)
                        return -EINVAL;
                status = poudland_v1_take_inbox(context, request_id,
                                                &message);
                if (status == 1)
                        return poudland_v1_consume_response(
                            pending, &message, output);
        }
        for (;;) {
                status = poudland_v1_read_message(
                    context, deadline, &message, &message_class);
                if (status != 0)
                        return status;
                if (message_class == POUDLAND_V1_CLASS_REQUEST)
                        return poudland_v1_fail(context, -EPROTO);
                if (message_class == POUDLAND_V1_CLASS_EVENT) {
                        if (request_id == 0) {
                                poudland_v1_copy_message(output, &message);
                                return 0;
                        }
                        status = poudland_v1_store_event(context, &message);
                        if (status != 0)
                                return status;
                        continue;
                }
                {
                        struct poudland_v1_pending *incoming_pending =
                            poudland_v1_find_pending(
                                context, message.header.request_id);

                        if (incoming_pending == NULL)
                                return poudland_v1_fail(context, -EPROTO);
                        status = poudland_v1_validate_response(
                            incoming_pending, &message);
                        if (status != 0)
                                return poudland_v1_fail(context, status);
                        if (pending != NULL &&
                            incoming_pending == pending)
                                return poudland_v1_consume_response(
                                    pending, &message, output);
                        status = poudland_v1_store_inbox(context, &message);
                        if (status != 0)
                                return status;
                }
        }
}

static int_32 poudland_v1_wait(struct poudland_v1_context *context,
                              uint_32 request_id, int_32 timeout_ms,
                              struct poudland_v1_message *output)
{
        struct poudland_v1_deadline deadline;
        int_32 status;

        if (context == NULL || output == NULL || timeout_ms < 0)
                return -EINVAL;
        if (!context->connected || context->fd < 0)
                return -ENOTCONN;
        status = poudland_v1_deadline_init(&deadline, timeout_ms);
        if (status != 0)
                return poudland_v1_fail(context, status);
        return poudland_v1_wait_until(context, request_id, &deadline,
                                      output);
}

int_32 poudland_v1_wait_response(struct poudland_v1_context *context,
                                uint_32 request_id, int_32 timeout_ms,
                                struct poudland_v1_message *response)
{
        if (request_id == 0)
                return -EINVAL;
        return poudland_v1_wait(context, request_id, timeout_ms, response);
}

int_32 poudland_v1_next_event(struct poudland_v1_context *context,
                             int_32 timeout_ms,
                             struct poudland_v1_message *event)
{
        return poudland_v1_wait(context, 0, timeout_ms, event);
}

int_32 poudland_v1_connect(struct poudland_v1_context *context,
                          const char *service, int_32 timeout_ms,
                          uint_32 client_capabilities)
{
        struct poudland_v1_deadline deadline;
        struct poudland_v1_hello hello = {
            .min_version = POUDLAND_V1_VERSION,
            .max_version = POUDLAND_V1_VERSION,
            .capabilities = client_capabilities,
        };
        struct poudland_v1_message response;
        struct poudland_v1_welcome *welcome;
        uint_32 request_id;
        int_32 remaining_ms;
        int_32 status;

        if (context == NULL || service == NULL || timeout_ms < 0)
                return -EINVAL;
        if (context->connected || context->fd >= 0)
                return -EALREADY;
        status = poudland_v1_deadline_init(&deadline, timeout_ms);
        if (status != 0)
                return status;
        for (;;) {
                status = frog_pkg_connect(service, true);
                if (status >= 0)
                        break;
                if (status != -ENOENT)
                        return status;
                status = poudland_v1_deadline_remaining(&deadline,
                                                        &remaining_ms);
                if (status != 0)
                        return status;
                if (remaining_ms == 0)
                        return -ETIMEDOUT;
                if (remaining_ms > POUDLAND_V1_CONNECT_RETRY_MS)
                        remaining_ms = POUDLAND_V1_CONNECT_RETRY_MS;
                status = wait2(NULL, 0, remaining_ms);
                if (status != 0)
                        return status < 0 ? status : -EIO;
        }
        context->fd = status;
        context->connected = 1;
        context->client_capabilities = client_capabilities;
        status = poudland_v1_begin_request(
            context, POUDLAND_V1_MSG_HELLO, &hello, sizeof(hello),
            &request_id);
        if (status != 0)
                return poudland_v1_fail(context, status);
        status = poudland_v1_wait_handshake_until(
            context, request_id, &deadline, &response);
        if (status != 0)
                return poudland_v1_fail(context, status);
        welcome = (struct poudland_v1_welcome *) response.payload;
        if (welcome->reserved != 0)
                return poudland_v1_fail(context, -EPROTO);
        if (welcome->selected_version < hello.min_version ||
            welcome->selected_version > hello.max_version ||
            welcome->selected_version != POUDLAND_V1_VERSION)
                return poudland_v1_fail(context, -EPROTONOSUPPORT);
        context->version = welcome->selected_version;
        context->display_width = welcome->display_width;
        context->display_height = welcome->display_height;
        context->server_capabilities = welcome->capabilities;
        return 0;
}

int_32 poudland_v1_window_create(
    struct poudland_v1_context *context,
    const struct poudland_v1_window_new *request, int_32 timeout_ms,
    struct poudland_v1_window_init *result)
{
        struct poudland_v1_message response;
        const struct poudland_v1_window_init *init;
        uint_32 request_id;
        int_32 status;

        if (context == NULL || request == NULL || result == NULL ||
            timeout_ms < 0)
                return -EINVAL;
        if (request->width == 0 || request->width > 4096U ||
            request->height == 0 || request->height > 4096U)
                return -EINVAL;
        status = poudland_v1_begin_request(
            context, POUDLAND_V1_MSG_WINDOW_NEW, request, sizeof(*request),
            &request_id);
        if (status != 0)
                return status;
        response.header.type = 0;
        status = poudland_v1_wait_response(context, request_id, timeout_ms,
                                           &response);
        if (status == -ETIMEDOUT &&
            response.header.type != POUDLAND_V1_MSG_ERROR)
                return poudland_v1_fail(context, status);
        if (status != 0)
                return status;
        init = (const struct poudland_v1_window_init *) response.payload;
        if (init->window_id == 0 || init->width == 0 ||
            init->width > 4096U || init->height == 0 ||
            init->height > 4096U)
                return poudland_v1_fail(context, -EPROTO);
        *result = *init;
        return 0;
}

int_32 poudland_v1_window_close(struct poudland_v1_context *context,
                               uint_32 window_id, int_32 timeout_ms)
{
        struct poudland_v1_window_close request = {
            .window_id = window_id,
        };
        struct poudland_v1_message response;
        const struct poudland_v1_window_closed *closed;
        uint_32 request_id;
        int_32 status;

        if (context == NULL || window_id == 0 || timeout_ms < 0)
                return -EINVAL;
        status = poudland_v1_begin_request(
            context, POUDLAND_V1_MSG_WINDOW_CLOSE, &request, sizeof(request),
            &request_id);
        if (status != 0)
                return status;
        response.header.type = 0;
        status = poudland_v1_wait_response(context, request_id, timeout_ms,
                                           &response);
        if (status == -ETIMEDOUT &&
            response.header.type != POUDLAND_V1_MSG_ERROR)
                return poudland_v1_fail(context, status);
        if (status != 0)
                return status;
        closed = (const struct poudland_v1_window_closed *) response.payload;
        if (closed->window_id != window_id)
                return poudland_v1_fail(context, -EPROTO);
        return 0;
}

int_32 poudland_v1_disconnect(struct poudland_v1_context *context)
{
        int_32 fd;
        int_32 status = 0;

        if (context == NULL)
                return -EINVAL;
        fd = context->fd;
        if (fd >= 0)
                status = close(fd);
        poudland_v1_context_init(context);
        return status;
}
