#ifndef __FROG_DEVICE_PS2_PORTS_H
#define __FROG_DEVICE_PS2_PORTS_H

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
#define AUX_BUF_SIZE 2048

#define BUSY_WAIT_TIME 5000

#include <frog/types.h>
#include <asm/io.h>
/**
 * Wait PS/2 controller's output buffer is filled.
 *
 * Use it before READING from the controller.
 * *****************************************************************************/
static inline int_8 ps2_wait_readable(void)
{
        uint_32 timeout = BUSY_WAIT_TIME;
        while (--timeout) {
                /* PS2_STR_OUTPUT_BUFFER_FULL; */
                if (inb(PS2_STATUS) & 0x1)
                        return 0;
        }
        return 1;
}

/**
 * Wait PS/2 controller's input buffer is filled.
 *
 * Use it before WRITING to the controller.
 *
 * return 0 means can not write 
 * return 1 means can write
 *****************************************************************************/
static inline int_8 ps2_wait_writeable(void)
{
        uint_32 timeout = BUSY_WAIT_TIME;
        while (--timeout) {
                /* PS2_STR_SEND_NOTREADY */
                if (inb(PS2_STATUS) & (0x1 << 1)) 
                        return 0;
        }
        return 1;
}


/**
 * Send a command with no response or argument
 *
 *****************************************************************************/
static inline void ps2_command(uint_8 cmd)
{
        ps2_wait_writeable();
        outb(PS2_COMMAND, cmd);
}

/**
 * Send a command with response
 *****************************************************************************/
static inline uint_8 ps2_command_response(uint_8 cmd)
{
        ps2_wait_writeable();
        outb(PS2_COMMAND, cmd);
        ps2_wait_readable();
        return inb(PS2_DATA);
}

/**
 * Send a command with argument but no response
 *****************************************************************************/
static inline void ps2_command_arg(uint_8 cmd, uint_8 arg)
{
        ps2_wait_writeable();
        outb(PS2_COMMAND, cmd);
        ps2_wait_readable();
        outb(PS2_DATA, arg);
}

/**
 * Read from ps2 data
 *****************************************************************************/
static inline uint_8 ps2_read_byte(void)
{
        ps2_wait_readable();
        return inb(PS2_DATA);
}

/**
 * Communicate with PS2 keyboard
 *****************************************************************************/
static inline uint_8 kbd_write(uint_8 data)
{
        ps2_wait_writeable();
        outb(PS2_DATA, data);
        ps2_wait_readable();
        return inb(PS2_DATA);
}

static inline uint_8 mouse_write(uint_8 data)
{
        ps2_command_arg(MOUSE_WRITE, data);
        ps2_wait_readable();
        return inb(PS2_DATA);
}

#endif
