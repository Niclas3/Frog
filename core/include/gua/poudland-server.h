#include <list.h>
#include <ostype.h>

#include <gua/2d_graphics.h>

typedef struct poundland_globals {
    /* Display resolution */
    unsigned int width;
    unsigned int height;
    uint_32 stride;

    /* Core graphics context */
    void *backend_framebuffer;
    gfx_context_t *backend_ctx;

    /* Mouse location */
    signed int mouse_x;
    signed int mouse_y;

    /*
     * Previous mouse location, so that events can have
     * both the new and old mouse location together
     */
    signed int last_mouse_x;
    signed int last_mouse_y;

    /* List of all windows */
    struct list_head *windows;

    /* Hash of window IDs to their objects */
    uint_32 *wids_to_windows;

    /*
     * Window stacking information
     * TODO: Support multiple top and bottom windows.
     */
    // yutani_server_window_t * bottom_z;
    // struct list_head * mid_zs;
    // struct list_head * menu_zs;
    // struct list_head * overlay_zs;
    // yutani_server_window_t * top_z;

    /* Damage region list */
    // list_t * update_list;

    /* Mouse cursors */
    sprite_t mouse_sprite;
    sprite_t mouse_sprite_drag;
    sprite_t mouse_sprite_resize_v;
    sprite_t mouse_sprite_resize_h;
    sprite_t mouse_sprite_resize_da;
    sprite_t mouse_sprite_resize_db;
    sprite_t mouse_sprite_point;
    sprite_t mouse_sprite_ibeam;

    /* Server backend communication identifier */
    char *server_ident;
    FILE *server;

    /* Pointer to focused window */
    yutani_server_window_t *focused_window;

    /* Mouse movement state */
    int mouse_state;

    /* Pointer to window being manipulated by mouse actions */
    yutani_server_window_t *mouse_window;

    /* Buffered information on mouse-moved window */
    int mouse_win_x;
    int mouse_win_y;
    int mouse_init_x;
    int mouse_init_y;
    int mouse_init_r;

    int_32 mouse_click_x_orig;
    int_32 mouse_click_y_orig;

    int mouse_drag_button;
    int mouse_moved;

    int_32 mouse_click_x;
    int_32 mouse_click_y;

    /* Pointer to window being resized */
    yutani_server_window_t *resizing_window;
    int_32 resizing_w;
    int_32 resizing_h;
    yutani_scale_direction_t resizing_direction;
    int_32 resizing_offset_x;
    int_32 resizing_offset_y;
    int resizing_button;

    /* List of clients subscribing to window information events */
    list_t *window_subscribers;

    /* When the server started, used for timing functions */
    time_t start_time;
    suseconds_t start_subtime;

    /* Pointer to last hovered window to allow exit events */
    yutani_server_window_t *old_hover_window;

    /* Key bindings */
    // hashmap_t *key_binds;

    /* Windows to remove after the end of the rendering pass */
    list_t *windows_to_remove;

    /* For nested mode, the host Yutani context and window */
    yutani_t *host_context;
    yutani_window_t *host_window;

    /* Map of clients to their windows */
    // hashmap_t *clients_to_windows;

    /* Toggles for debugging window locations */
    int debug_bounds;
    int debug_shapes;

    /* Next frame should resize host context */
    int resize_on_next;

    /* Last mouse buttons - used for some specialized mouse drivers */
    uint_32 last_mouse_buttons;


    /* Renderer plugin context */
    void *renderer_ctx;

    int reload_renderer;
    uint_8 active_modifiers;

    // uint64_t resize_release_time;
    int_32 resizing_init_w;
    int_32 resizing_init_h;

    // list_t *windows_to_minimize;
    // list_t *minimized_zs;
} yutani_globals_t;
