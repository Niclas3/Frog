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

static void add_damage(struct poudland_p0_protocol_result *result,
                       struct poudland_p0_rect rect)
{
        uint_32 index;

        if (rect.width <= 0 || rect.height <= 0)
                return;
        for (index = 0; index < result->damage_count; ++index) {
                const struct poudland_p0_rect *existing =
                    &result->damages[index];

                if (existing->x == rect.x && existing->y == rect.y &&
                    existing->width == rect.width &&
                    existing->height == rect.height)
                        return;
        }
        if (result->damage_count < POUDLAND_P0_PROTOCOL_DAMAGE_MAX)
                result->damages[result->damage_count++] = rect;
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

static void *prepare_event(struct poudland_p0_protocol *protocol,
                           struct poudland_p0_protocol_result *result,
                           uint_32 peer_id, uint_32 capability,
                           uint_32 type, uint_32 payload_size)
{
        struct poudland_p0_protocol_session *session =
            find_session(protocol, peer_id);
        struct poudland_p0_protocol_event *event;
        struct poudland_v1_header *header;

        /* HELLO capabilities are subscriptions to asynchronous event
         * classes; WELCOME separately advertises everything this server can
         * produce. */
        if (!session || (session->capabilities & capability) == 0 ||
            result->event_count >= POUDLAND_P0_PROTOCOL_EVENT_MAX ||
            payload_size > POUDLAND_V1_P0_PAYLOAD_MAX)
                return NULL;
        event = &result->events[result->event_count++];
        clear_bytes(event, sizeof(*event));
        event->peer_id = peer_id;
        event->size = POUDLAND_V1_HEADER_SIZE + payload_size;
        header = (struct poudland_v1_header *) event->data;
        header->magic = POUDLAND_V1_MAGIC;
        header->version = POUDLAND_V1_VERSION;
        header->header_size = POUDLAND_V1_HEADER_SIZE;
        header->type = type;
        header->request_id = 0;
        header->payload_size = payload_size;
        return event->data + POUDLAND_V1_HEADER_SIZE;
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
        session->capabilities = hello->capabilities &
                                POUDLAND_P0_SERVER_CAPABILITIES;
        prepare_reply(result, POUDLAND_V1_MSG_WELCOME,
                      header->request_id, sizeof(*welcome));
        welcome = (struct poudland_v1_welcome *)
            (result->reply + POUDLAND_V1_HEADER_SIZE);
        welcome->selected_version = POUDLAND_V1_VERSION;
        welcome->display_width = protocol->display_width;
        welcome->display_height = protocol->display_height;
        welcome->capabilities = POUDLAND_P0_SERVER_CAPABILITIES;
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
        window->z_index = protocol->window_count;
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
        add_damage(result, window->bounds);
        return 0;
}

static void destroy_window(struct poudland_p0_protocol *protocol,
                           struct poudland_p0_protocol_window *window,
                           struct poudland_p0_protocol_result *result)
{
        struct poudland_p0_protocol_session *owner;
        uint_32 removed_z;
        uint_32 index;

        if (!window || !window->active)
                return;
        removed_z = window->z_index;
        owner = find_session(protocol, window->owner_peer_id);
        add_damage(result, window->bounds);
        if (protocol->focused_window_id == window->id)
                protocol->focused_window_id = 0;
        if (protocol->dragged_window_id == window->id)
                protocol->dragged_window_id = 0;
        if (protocol->hovered_window_id == window->id)
                protocol->hovered_window_id = 0;
        clear_bytes(window, sizeof(*window));
        if (owner && owner->window_count != 0)
                owner->window_count--;
        if (protocol->window_count != 0)
                protocol->window_count--;
        for (index = 0; index < POUDLAND_V1_SERVER_WINDOW_MAX; ++index) {
                struct poudland_p0_protocol_window *other =
                    &protocol->windows[index];

                if (other->active && other->z_index > removed_z)
                        other->z_index--;
        }
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
        destroy_window(protocol, window, result);
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
                destroy_window(protocol, window, result);
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

static struct poudland_p0_protocol_window *hit_test(
    struct poudland_p0_protocol *protocol, int_32 x, int_32 y)
{
        struct poudland_p0_protocol_window *hit = NULL;
        uint_32 index;

        for (index = 0; index < POUDLAND_V1_SERVER_WINDOW_MAX; ++index) {
                struct poudland_p0_protocol_window *window =
                    &protocol->windows[index];
                int_64 right;
                int_64 bottom;

                if (!window->active)
                        continue;
                right = (int_64) window->bounds.x + window->bounds.width;
                bottom = (int_64) window->bounds.y + window->bounds.height;
                if ((int_64) x < window->bounds.x || (int_64) x >= right ||
                    (int_64) y < window->bounds.y || (int_64) y >= bottom)
                        continue;
                if (!hit || window->z_index > hit->z_index)
                        hit = window;
        }
        return hit;
}

static bool raise_window(struct poudland_p0_protocol *protocol,
                         struct poudland_p0_protocol_window *window)
{
        uint_32 old_z;
        uint_32 index;

        if (!window || protocol->window_count == 0 ||
            window->z_index == protocol->window_count - 1U)
                return false;
        old_z = window->z_index;
        for (index = 0; index < POUDLAND_V1_SERVER_WINDOW_MAX; ++index) {
                struct poudland_p0_protocol_window *other =
                    &protocol->windows[index];

                if (other->active && other != window &&
                    other->z_index > old_z)
                        other->z_index--;
        }
        window->z_index = protocol->window_count - 1U;
        return true;
}

static void emit_pointer_event(
    struct poudland_p0_protocol *protocol,
    struct poudland_p0_protocol_result *result,
    const struct poudland_p0_protocol_window *window, uint_32 type,
    uint_32 button, uint_32 buttons, int_32 screen_x, int_32 screen_y)
{
        struct poudland_v1_pointer_event *event;

        if (!window)
                return;
        event = prepare_event(protocol, result, window->owner_peer_id,
                              POUDLAND_V1_CAP_POINTER,
                              POUDLAND_V1_MSG_POINTER_EVENT,
                              sizeof(*event));
        if (!event)
                return;
        event->window_id = window->id;
        event->screen_x = screen_x;
        event->screen_y = screen_y;
        event->local_x = screen_x - window->bounds.x;
        event->local_y = screen_y - window->bounds.y;
        event->type = type;
        event->button = button;
        event->buttons = buttons;
}

static void update_hover(
    struct poudland_p0_protocol *protocol,
    struct poudland_p0_protocol_result *result,
    struct poudland_p0_protocol_window *window,
    int_32 screen_x, int_32 screen_y, uint_32 buttons)
{
        struct poudland_p0_protocol_window *old =
            find_window(protocol, protocol->hovered_window_id);

        if (old == window)
                return;
        emit_pointer_event(protocol, result, old,
                           POUDLAND_V1_POINTER_LEAVE, 0, buttons,
                           screen_x, screen_y);
        protocol->hovered_window_id = window ? window->id : 0;
        emit_pointer_event(protocol, result, window,
                           POUDLAND_V1_POINTER_ENTER, 0, buttons,
                           screen_x, screen_y);
}

static int_32 clamp_coordinate(int_64 coordinate, int_64 minimum,
                               int_64 maximum)
{
        if (coordinate < minimum)
                return (int_32) minimum;
        if (coordinate > maximum)
                return (int_32) maximum;
        return (int_32) coordinate;
}

static bool move_dragged_window(
    struct poudland_p0_protocol *protocol,
    struct poudland_p0_protocol_window *window, int_32 screen_x,
    int_32 screen_y, struct poudland_p0_protocol_result *result)
{
        struct poudland_v1_window_configure *configure;
        struct poudland_p0_rect old_bounds = window->bounds;
        int_64 maximum_x = protocol->display_width == 0
                              ? 0
                              : (int_64) protocol->display_width - 1;
        int_64 maximum_y = protocol->display_height == 0
                              ? 0
                              : (int_64) protocol->display_height - 1;
        int_64 minimum_x = 1 - (int_64) window->bounds.width;
        int_64 minimum_y = 1 - (int_64) window->bounds.height;

        window->bounds.x = clamp_coordinate(
            (int_64) screen_x - protocol->drag_offset_x,
            minimum_x, maximum_x);
        window->bounds.y = clamp_coordinate(
            (int_64) screen_y - protocol->drag_offset_y,
            minimum_y, maximum_y);
        if (window->bounds.x == old_bounds.x &&
            window->bounds.y == old_bounds.y)
                return false;
        add_damage(result, old_bounds);
        add_damage(result, window->bounds);
        configure = prepare_event(
            protocol, result, window->owner_peer_id,
            POUDLAND_V1_CAP_CONFIGURE,
            POUDLAND_V1_MSG_WINDOW_CONFIGURE, sizeof(*configure));
        if (configure) {
                configure->window_id = window->id;
                configure->x = window->bounds.x;
                configure->y = window->bounds.y;
        }
        return true;
}

int_32 poudland_p0_protocol_handle_pointer(
    struct poudland_p0_protocol *protocol, int_32 screen_x,
    int_32 screen_y, uint_32 buttons,
    struct poudland_p0_protocol_result *result)
{
        struct poudland_p0_protocol_window *window;
        struct poudland_p0_protocol_window *old_focus;
        bool left_was_down;
        bool left_is_down;
        bool raised;

        if (!protocol || !result)
                return -EINVAL;
        clear_bytes(result, sizeof(*result));
        left_was_down =
            (protocol->pointer_buttons & POUDLAND_P0_BUTTON_LEFT) != 0;
        left_is_down = (buttons & POUDLAND_P0_BUTTON_LEFT) != 0;
        protocol->pointer_x = screen_x;
        protocol->pointer_y = screen_y;
        protocol->pointer_buttons = buttons;

        if (!left_was_down && left_is_down) {
                window = hit_test(protocol, screen_x, screen_y);
                update_hover(protocol, result, window, screen_x,
                             screen_y, buttons);
                old_focus = find_window(protocol,
                                        protocol->focused_window_id);
                if (!window) {
                        if (old_focus)
                                add_damage(result, old_focus->bounds);
                        protocol->focused_window_id = 0;
                        protocol->dragged_window_id = 0;
                        return 0;
                }
                if (old_focus != window) {
                        if (old_focus)
                                add_damage(result, old_focus->bounds);
                        add_damage(result, window->bounds);
                }
                raised = raise_window(protocol, window);
                if (raised)
                        add_damage(result, window->bounds);
                protocol->focused_window_id = window->id;
                protocol->dragged_window_id = window->id;
                protocol->drag_offset_x = screen_x - window->bounds.x;
                protocol->drag_offset_y = screen_y - window->bounds.y;
                emit_pointer_event(protocol, result, window,
                                   POUDLAND_V1_POINTER_DOWN,
                                   POUDLAND_P0_BUTTON_LEFT, buttons,
                                   screen_x, screen_y);
                if (raised)
                        emit_pointer_event(protocol, result, window,
                                           POUDLAND_V1_POINTER_RAISE,
                                           POUDLAND_P0_BUTTON_LEFT,
                                           buttons, screen_x, screen_y);
                return 0;
        }

        if (left_was_down && !left_is_down) {
                bool moved = false;

                window = find_window(protocol,
                                     protocol->dragged_window_id);
                if (window)
                        moved = move_dragged_window(
                            protocol, window, screen_x, screen_y, result);
                update_hover(protocol, result,
                             hit_test(protocol, screen_x, screen_y),
                             screen_x, screen_y, buttons);
                if (window && moved)
                        emit_pointer_event(protocol, result, window,
                                           POUDLAND_V1_POINTER_DRAG, 0,
                                           buttons, screen_x, screen_y);
                emit_pointer_event(protocol, result, window,
                                   POUDLAND_V1_POINTER_CLICK,
                                   POUDLAND_P0_BUTTON_LEFT, buttons,
                                   screen_x, screen_y);
                protocol->dragged_window_id = 0;
                return 0;
        }

        if (left_is_down) {
                bool moved = false;

                window = find_window(protocol,
                                     protocol->dragged_window_id);
                if (window)
                        moved = move_dragged_window(
                            protocol, window, screen_x, screen_y, result);
                update_hover(protocol, result,
                             hit_test(protocol, screen_x, screen_y),
                             screen_x, screen_y, buttons);
                if (window && moved)
                        emit_pointer_event(protocol, result, window,
                                           POUDLAND_V1_POINTER_DRAG, 0,
                                           buttons, screen_x, screen_y);
                return 0;
        }

        window = hit_test(protocol, screen_x, screen_y);
        update_hover(protocol, result, window, screen_x, screen_y,
                     buttons);
        emit_pointer_event(protocol, result, window,
                           POUDLAND_V1_POINTER_MOVE, 0, buttons,
                           screen_x, screen_y);
        return 0;
}

int_32 poudland_p0_protocol_handle_key(
    struct poudland_p0_protocol *protocol, uint_8 key,
    struct poudland_p0_protocol_result *result)
{
        struct poudland_p0_protocol_window *window;
        struct poudland_v1_key_event *event;

        if (!protocol || !result)
                return -EINVAL;
        clear_bytes(result, sizeof(*result));
        window = find_window(protocol, protocol->focused_window_id);
        if (!window)
                return 0;
        window->last_key = key;
        event = prepare_event(protocol, result, window->owner_peer_id,
                              POUDLAND_V1_CAP_KEYBOARD,
                              POUDLAND_V1_MSG_KEY_EVENT,
                              sizeof(*event));
        if (!event)
                return 0;
        event->window_id = window->id;
        event->keycode = key;
        event->action = POUDLAND_V1_KEY_PRESS;
        event->modifiers = 0;
        event->codepoint = key;
        return 0;
}
