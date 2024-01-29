#pragma once
#include <ostype.h>
#include <list.h>
#include <gua/2d_graphics.h>
typedef uint_32 poudland_wid_t;

/*
 * Server connection context.
 */
typedef struct poudland_context {
	void * sock;

	/* server display size */
	uint_32 display_width;
	uint_32 display_height;

	/* Hash of window IDs to window objects */
	// hashmap_t * windows;

	/* queued events */
	struct list_head equeue;

	/* server identifier string */
	char * server_ident;
} poudland_t;

typedef struct poudland_window {
	/* Server window identifier, unique to each window */
	poudland_wid_t wid;

	/* Window size */
	uint_32 width;
	uint_32 height;

	/* Window backing buffer */
	char * buffer;
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
	void * user_data;

	/* Window position in the server; automatically updated */
	int_32 x;
	int_32 y;

	/* Flags for the decorator library to use */
	uint_32 decorator_flags;

	/* Server context that owns this window */
	poudland_t * ctx;

	int_32 mouse_state;
} poudland_window_t;

gfx_context_t *init_graphics_poudland_double_buffer(poudland_window_t *window);
