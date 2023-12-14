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

//Mouse
#define MOUSE_WRITE      0xd4
#define MOUSE_ENABLE     0xf4
//keyboard


//-----------------------------------------------
// PS2 state
//-----------------------------------------------
// TODO: finish all keyboard state
#define PS2_STR_OUTPUT_BUFFER_FULL 0x01
#define PS2_STR_SEND_NOTREADY      0x02

void ps2hid_init(void);
void inthandler2C(void);

#endif
