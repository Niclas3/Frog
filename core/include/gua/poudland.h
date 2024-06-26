#pragma once
#include <gua/2d_graphics.h>
#include <hashmap.h>
#include <hid/mouse.h>
#include <list.h>
#include <ostype.h>

// typedef uint_32 poudland_wid_t;

/* Magic value */
#define PL_MSG__MAGIC 0xABAD1DEA

// All message types
/* Client messages */
#define PL_MSG_HELLO 0x00000001
#define PL_MSG_WINDOW_NEW 0x00000002
#define PL_MSG_FLIP 0x00000003
#define PL_MSG_KEY_EVENT 0x00000004
#define PL_MSG_MOUSE_EVENT 0x00000005
#define PL_MSG_WINDOW_MOVE 0x00000006
#define PL_MSG_WINDOW_CLOSE 0x00000007
#define PL_MSG_WINDOW_SHOW 0x00000008
#define PL_MSG_WINDOW_HIDE 0x00000009
#define PL_MSG_WINDOW_STACK 0x0000000A
#define PL_MSG_WINDOW_FOCUS_CHANGE 0x0000000B
#define PL_MSG_WINDOW_MOUSE_EVENT 0x0000000C
#define PL_MSG_FLIP_REGION 0x0000000D
#define PL_MSG_WINDOW_NEW_FLAGS 0x0000000E

#define PL_MSG_RESIZE_REQUEST 0x00000010
#define PL_MSG_RESIZE_OFFER 0x00000011
#define PL_MSG_RESIZE_ACCEPT 0x00000012
#define PL_MSG_RESIZE_BUFID 0x00000013
#define PL_MSG_RESIZE_DONE 0x00000014

#define PL_MSG_WINDOW_MOVE_RELATIVE 0x00000015

/* Some session management / de stuff */
#define PL_MSG_WINDOW_ADVERTISE 0x00000020
#define PL_MSG_SUBSCRIBE 0x00000021
#define PL_MSG_UNSUBSCRIBE 0x00000022
#define PL_MSG_NOTIFY 0x00000023
#define PL_MSG_QUERY_WINDOWS 0x00000024
#define PL_MSG_WINDOW_FOCUS 0x00000025
#define PL_MSG_WINDOW_DRAG_START 0x00000026
#define PL_MSG_WINDOW_WARP_MOUSE 0x00000027
#define PL_MSG_WINDOW_SHOW_MOUSE 0x00000028
#define PL_MSG_WINDOW_RESIZE_START 0x00000029
#define PL_MSG_WINDOW_PANEL_SIZE 0x0000002a
#define PL_MSG_SESSION_END 0x00000030
#define PL_MSG_KEY_BIND 0x00000040

#define PL_MSG_WINDOW_UPDATE_SHAPE 0x00000050

#define PL_MSG_CLIPBOARD 0x00000060

#define PL_MSG_GOODBYE 0x000000F0

/* Special request (eg. one-off single-shot requests like "please maximize me"
 */
#define PL_MSG_SPECIAL_REQUEST 0x00000100

/* Server responses */
#define PL_MSG_WELCOME 0x00010001      // response client PL_MSG_HELLO
#define PL_MSG_WINDOW_INIT 0x00010002  // response client PL_MSG_WINDOW_NEW


// Mouse click define

#define POUDLAND_MOUSE_LEFT_CLICK LEFT_CLICK
#define POUDLAND_MOUSE_RIGHT_CLICK RIGHT_CLICK
#define POUDLAND_MOUSE_MIDDLE_CLICK MIDDLE_CLICK
#define POUDLAND_MOUSE_SCROLL_UP MOUSE_SCROLL_UP
#define POUDLAND_MOUSE_SCROLL_DOWN MOUSE_SCROLL_DOWN

// Mouse status
/*
 * MOUSE_EVENT
 *
 * Mouse events have different types.
 *
 * Most of these should be self-explanatory.
 *
 * CLICK: A down-up click has occured.
 * DRAG: The mouse is down and moving.
 * RAISE: A mouse button was released.
 * DOWN: A mouse button has been pressed.
 * MOVE: The mouse has moved without a mouse button pressed.
 * LEAVE: The mouse has left the given window.
 * ENTER: The mouse has entered the given window.
 */
#define POUDLAND_MOUSE_EVENT_CLICK 0
#define POUDLAND_MOUSE_EVENT_DRAG 1
#define POUDLAND_MOUSE_EVENT_RAISE 2
#define POUDLAND_MOUSE_EVENT_DOWN 3
#define POUDLAND_MOUSE_EVENT_MOVE 4
#define POUDLAND_MOUSE_EVENT_LEAVE 5
#define POUDLAND_MOUSE_EVENT_ENTER 6

// Mouse states
// Normal stands for still mouse
// Moving stands for moving mouse but no click
// Dragging stands for moving mouse with click
// Resizing and Rotating are not support now
#define POUDLAND_MOUSE_STATE_NORMAL 0
#define POUDLAND_MOUSE_STATE_MOVING 1
#define POUDLAND_MOUSE_STATE_DRAGGING 2
#define POUDLAND_MOUSE_STATE_RESIZING 3
#define POUDLAND_MOUSE_STATE_ROTATING 4

typedef struct poudland_message {
    uint_32 magic;
    uint_32 type;
    uint_32 size;
    struct list_head target;
    uint_8 body[];
} poudland_msg_t;

/*
 * Server connection context for each client
 * like a session
 */
typedef struct poudland_context {
    void *sock;

    /* server display size */
    uint_32 display_width;
    uint_32 display_height;

    /* Hash of window IDs to window objects */
    // all windows in clients session
    hashmap_t *windows;

    /* queued events */
    struct list_head equeue;  // a message list

    /* server identifier string */
    char *server_ident;
} poudland_t;

typedef struct poudland_window {
    /* Server window identifier, unique to each window */
    uint_32 wid;

    /* Window size */
    uint_32 width;
    uint_32 height;

    /* Window backing buffer */
    char *buffer;
    /*
     * Because the buffer can change during resizing,
     * buffers are indexed to ensure we are using
     * the one the server expects.
     */
    uint_32 bufid;

    /* Window focused flag */
    uint_8 focused;

    /* Old buffer ID */
    uint_32 oldbufid;

    /* Generic pointer for client use */
    void *user_data;

    /* Window position in the server; automatically updated */
    int_32 x;
    int_32 y;

    /* Flags for the decorator library to use */
    uint_32 decorator_flags;

    /* Server context that owns this window */
    poudland_t *ctx;

    int_32 mouse_state;
} poudland_window_t;

struct poudland_hello_msg_t {
    uint_32 width;
    uint_32 height;
};

struct poudland_msg_window_new {
    uint_32 pos_x;
    uint_32 pos_y;
    uint_32 width;
    uint_32 height;
    uint_32 color;
};

struct poudland_msg_window_new_init {
    uint_32 wid;
    uint_32 pos_x;
    uint_32 pos_y;
    uint_32 width;
    uint_32 height;
    uint_32 color;
};

struct poudland_msg_window_close {
    uint_32 wid;
};


struct poudland_msg_mouse_event {
    uint_32 wid;
    int_32  x;
    int_32  y;
    uint_32 mouse_event_type;
    uint_32 button_down;  // which button down
};
struct poudland_msg_win_move {
    uint_32 win_id;
    uint_32 x;
    uint_32 y;
};

poudland_msg_t *poudland_wait_for(poudland_t *p, uint_32 type);
poudland_t *poudland_init_context(const char *server);
gfx_context_t *init_graphics_poudland_double_buffer(poudland_window_t *window);

uint_32 poudland_create_window(poudland_t *ctx,
                               int_32 fd,
                               int_32 color,
                               int_32 x,
                               int_32 y,
                               int_32 width,
                               int_32 height);
void poudland_remove_window(int_32 fd, int_32 wid);

void poudland_move_window(int_32 serverfd, int wid, int x, int y);
