#ifndef __HID_MOUSE_H
#define __HID_MOUSE_H
#include <ostype.h>
typedef enum {
	LEFT_CLICK   = 0x01,
	RIGHT_CLICK  = 0x02,
	MIDDLE_CLICK = 0x04,

	MOUSE_SCROLL_UP = 0x10,
	MOUSE_SCROLL_DOWN = 0x20,
} mouse_click_t;

typedef struct {
	uint_32 magic;
	int_32 x_difference;
	int_32 y_difference;
	mouse_click_t buttons;
} mouse_device_packet_t;

#define MOUSE_MAGIC 0xFEED1234

#endif
