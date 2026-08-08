#ifndef _GUA_POUDLAND_PROTOCOL_V1_H
#define _GUA_POUDLAND_PROTOCOL_V1_H

#include <frog/types.h>

#define POUDLAND_V1_MAGIC              0x504c5631U
#define POUDLAND_V1_VERSION            1U
#define POUDLAND_V1_HEADER_SIZE        20U
#define POUDLAND_V1_P0_PAYLOAD_MAX     64U
#define POUDLAND_V1_P0_MESSAGE_MAX     \
        (POUDLAND_V1_HEADER_SIZE + POUDLAND_V1_P0_PAYLOAD_MAX)
#define POUDLAND_V1_SESSION_WINDOW_MAX 16U
#define POUDLAND_V1_SERVER_WINDOW_MAX  64U

#define POUDLAND_V1_CAP_CONFIGURE      0x00000001U
#define POUDLAND_V1_CAP_POINTER        0x00000002U
#define POUDLAND_V1_CAP_KEYBOARD       0x00000004U

enum poudland_v1_message_type {
        POUDLAND_V1_MSG_HELLO = 0x00000001U,
        POUDLAND_V1_MSG_WINDOW_NEW = 0x00000002U,
        POUDLAND_V1_MSG_WINDOW_CLOSE = 0x00000007U,

        POUDLAND_V1_MSG_WELCOME = 0x00010001U,
        POUDLAND_V1_MSG_WINDOW_INIT = 0x00010002U,
        POUDLAND_V1_MSG_WINDOW_CLOSED = 0x00010007U,
        POUDLAND_V1_MSG_ERROR = 0x000100ffU,

        POUDLAND_V1_MSG_WINDOW_CONFIGURE = 0x00020001U,
        POUDLAND_V1_MSG_POINTER_EVENT = 0x00020002U,
        POUDLAND_V1_MSG_KEY_EVENT = 0x00020003U,
};

enum poudland_v1_pointer_type {
        POUDLAND_V1_POINTER_CLICK = 1,
        POUDLAND_V1_POINTER_DOWN,
        POUDLAND_V1_POINTER_RAISE,
        POUDLAND_V1_POINTER_ENTER,
        POUDLAND_V1_POINTER_LEAVE,
        POUDLAND_V1_POINTER_MOVE,
        POUDLAND_V1_POINTER_DRAG,
};

enum poudland_v1_key_action {
        POUDLAND_V1_KEY_PRESS = 1,
        POUDLAND_V1_KEY_RELEASE,
        POUDLAND_V1_KEY_REPEAT,
};

struct poudland_v1_header {
        uint_32 magic;
        uint_16 version;
        uint_16 header_size;
        uint_32 type;
        uint_32 request_id;
        uint_32 payload_size;
};

struct poudland_v1_hello {
        uint_16 min_version;
        uint_16 max_version;
        uint_32 capabilities;
};

struct poudland_v1_welcome {
        uint_16 selected_version;
        uint_16 reserved;
        uint_32 display_width;
        uint_32 display_height;
        uint_32 capabilities;
};

struct poudland_v1_error {
        int_32 status;
        uint_32 failed_type;
};

struct poudland_v1_window_new {
        int_32 x;
        int_32 y;
        uint_32 width;
        uint_32 height;
        uint_32 xrgb8888;
};

struct poudland_v1_window_init {
        uint_32 window_id;
        int_32 x;
        int_32 y;
        uint_32 width;
        uint_32 height;
        uint_32 xrgb8888;
};

struct poudland_v1_window_close {
        uint_32 window_id;
};

struct poudland_v1_window_closed {
        uint_32 window_id;
};

struct poudland_v1_window_configure {
        uint_32 window_id;
        int_32 x;
        int_32 y;
};

struct poudland_v1_pointer_event {
        uint_32 window_id;
        int_32 screen_x;
        int_32 screen_y;
        int_32 local_x;
        int_32 local_y;
        uint_32 type;
        uint_32 button;
        uint_32 buttons;
};

struct poudland_v1_key_event {
        uint_32 window_id;
        uint_32 keycode;
        uint_32 action;
        uint_32 modifiers;
        uint_32 codepoint;
};

typedef char poudland_v1_header_must_be_20_bytes[
    sizeof(struct poudland_v1_header) == POUDLAND_V1_HEADER_SIZE ? 1 : -1];
typedef char poudland_v1_hello_must_be_8_bytes[
    sizeof(struct poudland_v1_hello) == 8U ? 1 : -1];
typedef char poudland_v1_welcome_must_be_16_bytes[
    sizeof(struct poudland_v1_welcome) == 16U ? 1 : -1];
typedef char poudland_v1_error_must_be_8_bytes[
    sizeof(struct poudland_v1_error) == 8U ? 1 : -1];
typedef char poudland_v1_window_new_must_be_20_bytes[
    sizeof(struct poudland_v1_window_new) == 20U ? 1 : -1];
typedef char poudland_v1_window_init_must_be_24_bytes[
    sizeof(struct poudland_v1_window_init) == 24U ? 1 : -1];
typedef char poudland_v1_window_close_must_be_4_bytes[
    sizeof(struct poudland_v1_window_close) == 4U ? 1 : -1];
typedef char poudland_v1_window_closed_must_be_4_bytes[
    sizeof(struct poudland_v1_window_closed) == 4U ? 1 : -1];
typedef char poudland_v1_window_configure_must_be_12_bytes[
    sizeof(struct poudland_v1_window_configure) == 12U ? 1 : -1];
typedef char poudland_v1_pointer_event_must_be_32_bytes[
    sizeof(struct poudland_v1_pointer_event) == 32U ? 1 : -1];
typedef char poudland_v1_key_event_must_be_20_bytes[
    sizeof(struct poudland_v1_key_event) == 20U ? 1 : -1];

#endif
