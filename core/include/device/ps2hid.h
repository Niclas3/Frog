#ifndef __SYS_PS2_HID_H
#define __SYS_PS2_HID_H
#include <ostype.h>

#define PS2_STATUS           0x64
#define PS2_COMMAND          0x64
#define PS2_DATA             0x60

#define PS2_DISABLE_PORT2  0xA7
#define PS2_ENABLE_PORT2   0xA8
#define PS2_DISABLE_PORT1  0xAD
#define PS2_ENABLE_PORT1   0xAE

#define PS2_READ_CONFIG    0x20
#define PS2_WRITE_CONFIG   0x60

//Mouse
#define MOUSE_WRITE      0xd4
#define MOUSE_ENABLE     0xf4
#define MOUSE_V_BIT        0x08

#define MOUSE_SET_REMOTE   0xF0
#define MOUSE_DEVICE_ID    0xF2
#define MOUSE_SAMPLE_RATE  0xF3
#define MOUSE_DATA_ON      0xF4
#define MOUSE_DATA_OFF     0xF5
#define MOUSE_SET_DEFAULTS 0xF6

#define MOUSE_DEFAULT         0
#define MOUSE_SCROLLWHEEL     1
#define MOUSE_BUTTONS         2

//keyboard
#define KBD_WRITE      0x60
#define KBDC_MODE             0x47
#define KBD_SET_SCANCODE   0xF0
//-----------------------------------------------
// PS2 state
//-----------------------------------------------
// TODO: finish all keyboard state
#define PS2_STR_OUTPUT_BUFFER_FULL 0x01
#define PS2_STR_SEND_NOTREADY      0x02

void ps2hid_init(void);

void inthandler2C(void);
void inthandler21(void);

#endif
