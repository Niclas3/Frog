#include <list.h>
#include <ostype.h>
#include <gua/2d_graphics.h>
#include <hashmap.h>

/* Mouse resolution scaling */
#define MOUSE_SCALE 3
#define INCOMING_MOUSE_SCALE * 3

/* Mouse cursor hotspot */
#define MOUSE_OFFSET_X 24
#define MOUSE_OFFSET_Y 23

/* Mouse cursor size */
#define MOUSE_WIDTH 64
#define MOUSE_HEIGHT 64

/*damage region*/
typedef struct {
    struct list_head damage_region_target;
    int_32 x;
    int_32 y;
    uint_32 width;
    uint_32 height;
} damage_rect_t;

// typedef uint_32 poudland_wid_t;
/*
 * Server window definitions
 */
typedef struct poudland_server_window {
	/* Window identifier number */
	uint_32 wid;

	/* Window location */
	signed long x;
	signed long y;

	/* Stack order */
	unsigned short z;

	/* Window size */
	int_32 width;
	int_32 height;

	/* Canvas buffer */
	uint_8 * buffer;
	uint_32 bufid;
	uint_32 newbufid;
	uint_8 * newbuffer;

	/* Connection that owns this window */
	uint_32 *owner;

	/* Rotation of windows XXX */
	int_16  rotation;

	/* Client advertisements */
	uint_32 client_flags;
	uint_32 client_icon;
	uint_32 client_length;
	char *  client_strings;

	/* Alpha shaping threshold */
	int alpha_threshold;

	/*
	 * Mouse cursor selection
	 * Originally, this specified whether the mouse was
	 * hidden, but it plays double duty since client
	 * control over mouse cursors was added.
	 */
	int show_mouse;
	int default_mouse;

	/* Tiling / untiling information */
	int tiled;
	int_32 untiled_width;
	int_32 untiled_height;
	int_32 untiled_left;
	int_32 untiled_top;

        /* list target mid_zs */
        struct list_head server_w_mid_target;
        /* list target for all windows list*/
        struct list_head server_w_target;

	/* Client-configurable server behavior flags */
	uint_32 server_flags;

	/* Window opacity */
	int opacity;

	/* Window is hidden? */
	int hidden;
	int minimized;

	int_32 icon_x, icon_y, icon_w, icon_h;
} poudland_server_window_t;

// lots of server things in server side
typedef struct poudland_globals {
    /* Display resolution */
    unsigned int width;
    unsigned int height;
    uint_32 stride;

    /*server fd*/
    uint_32 server_fd;

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
    /* insert at create a server windows*/
    struct list_head windows;

    /* Hash of window IDs to their objects */
    hashmap_t *wids_to_windows;

    /* Map of clients to their windows */
    // contains all clients on this server.
    struct list_head clients;

    /*
     * Window stacking information
     * TODO: Support multiple top and bottom windows.
     */
    poudland_server_window_t * bottom_z; // bottom windows

    struct list_head mid_zs;    /* regular windows list zs == z-axis-stack */
    struct list_head menu_zs;
    struct list_head overlay_zs;

    poudland_server_window_t * top_z;

    /* Damage region list */
    struct list_head update_list;  /* damage_region_target  windows list */

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
    // FILE *server;
    void *server;

    /* Pointer to focused window */
    poudland_server_window_t *focused_window;

    /* Mouse movement state */
    int mouse_state;

    /* Pointer to window being manipulated by mouse actions */
    poudland_server_window_t *mouse_window;

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
    poudland_server_window_t *resizing_window;
    int_32 resizing_w;
    int_32 resizing_h;
    // yutani_scale_direction_t resizing_direction;
    int_32 resizing_offset_x;
    int_32 resizing_offset_y;
    int resizing_button;

    /* List of clients subscribing to window information events */
    struct list_head window_subscribers;

    /* When the server started, used for timing functions */
    time_t start_time;
    suseconds_t start_subtime;

    /* Pointer to last hovered window to allow exit events */
    // for mouse 
    poudland_server_window_t *old_hover_window;

    /* Key bindings */
    // hashmap_t *key_binds;

    /* Windows to remove after the end of the rendering pass */
    // struct list_head windows_to_remove;

    /* For nested mode, the host Yutani context and window */
    // yutani_t *host_context;
    // yutani_window_t *host_window;


    /* Toggles for debugging window locations */
    int debug_bounds;
    int debug_shapes;

    /* Next frame should resize host context */
    int resize_on_next;

    /* Last mouse buttons - used for some specialized mouse drivers */
    uint_32 last_mouse_buttons;

    /* Renderer plugin context */
    void *renderer_ctx;

    // int reload_renderer;
    uint_8 active_modifiers;

    // uint64_t resize_release_time;
    int_32 resizing_init_w;
    int_32 resizing_init_h;

    // list_t *windows_to_minimize;
    // list_t *minimized_zs;
} poudland_globals_t;
