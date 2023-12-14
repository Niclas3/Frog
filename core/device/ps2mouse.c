#include <hid/keyboard.h>
#include <hid/ps2mouse.h>
#include <io.h>
#include <sys/pic.h>

#include <ioqueue.h>
#include <ostype.h>

#include <protect.h>
#include <sys/int.h>

// test
#include <sys/2d_graphics.h>


CircleQueue mouse_queue;
struct mouse_data {
    int_32 x;
    int_32 y;
    int_32 btn;
};

struct mouse_raw_data {
    uint_8 buf[3];
    uint_8 stage;  // mouse decoding process
    struct mouse_data cooked_mdata;
};

int mouse_decode(struct mouse_raw_data *mdata, uint_8 code);
void inthandler2C(void);

/**
 * Wait PS/2 controller's output buffer is filled.
 *
 * Use it before READING from the controller.
 * *****************************************************************************/
static int_8 ps2_wait_output(void)
{
    uint_32 timeout = 100000;
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
 *****************************************************************************/
static int_8 ps2_wait_input(void)
{
    uint_32 timeout = 100000;
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
static void ps2_command(uint_8 cmd)
{
    ps2_wait_input();
    outb(PS2_COMMAND, cmd);
}

/**
 * Send a command with response
 *****************************************************************************/
static uint_8 ps2_command_response(uint_8 cmd)
{
    ps2_wait_input();
    outb(PS2_COMMAND, cmd);
    ps2_wait_output();
    return inb(PS2_DATA);
}

/**
 * Send a command with argument but no response
 *****************************************************************************/
static void ps2_command_arg(uint_8 cmd, uint_8 arg)
{
    ps2_wait_input();
    outb(PS2_COMMAND, cmd);
    ps2_wait_output();
    outb(PS2_DATA, arg);
}

/**
 * Read from ps2 data 
 *****************************************************************************/
static uint_8 ps2_read_byte(void){
    ps2_wait_output();
    return inb(PS2_DATA);
}

/**
 * Communicate with PS2 keyboard
 *****************************************************************************/
static uint_8 kbd_write(uint_8 data){
    ps2_wait_input();
    outb(PS2_DATA, data);
    ps2_wait_output();
    return inb(PS2_DATA);
}

static uint_8 mouse_write(uint_8 data){
    ps2_command_arg(MOUSE_WRITE, data);
    ps2_wait_output();
    return inb(PS2_DATA);
}

void enable_mouse()
{
    /* ps2_wait_input(); */
    /* outb(PORT_KEYCOMMD, MOUSE_WRITE); */
    /* ps2_wait_input(); */
    /* outb(PORT_KEYDATE, MOUSE_ENABLE); */

    register_r0_intr_handler(INT_VECTOR_PS2_MOUSE, inthandler2C);
    ps2_wait_input();
    outb(PORT_KEYCOMMD, MOUSE_WRITE);
    ps2_wait_input();
    outb(PORT_KEYDATE, MOUSE_ENABLE);
    return;
}

int mouse_decode(struct mouse_raw_data *mdata, uint_8 code)
{
    if (mdata->stage == 0) {
        if (code == 0xfa) {
            mdata->stage = 1;
        }
        return 0;
    } else if (mdata->stage == 1) {
        if ((code & 0xc8) == 0x08) {
            mdata->buf[0] = code;
            mdata->stage = 2;
        }
        return 0;
    } else if (mdata->stage == 2) {
        mdata->buf[1] = code;
        mdata->stage = 3;
        return 0;
    } else if (mdata->stage == 3) {
        mdata->buf[2] = code;
        mdata->stage = 1;
        mdata->cooked_mdata.btn = mdata->buf[0] & 0x07;
        mdata->cooked_mdata.x = mdata->buf[1];
        mdata->cooked_mdata.y = mdata->buf[2];
        return 1;
    }
    return -1;
}

/* int 0x2C;
 * Interrupt handler for PS/2 mouse
 **/
struct mouse_raw_data mrd = {0};
void inthandler2C(void)
{
    char code = inb(PORT_KEYDATE);
    int sucs = mouse_decode(&mrd, code);

    if (sucs) {
        struct queue_data qdata = {
            .data = mrd.cooked_mdata.y,
            /* .data = code */
        };
        /* draw_2d_gfx_cursor(mrd.cooked_mdata.x, mrd.cooked_mdata.y); */

        /* ioqueue_put_data(&qdata, &mouse_queue); */
    }

    return;
}
