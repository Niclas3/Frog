#include <device/ps2hid.h>
#include <hid/keymap.h>

#include <io.h>
#include <ioqueue.h>

#include <protect.h>
#include <sys/int.h>

#include <debug.h>
#include <sys/2d_graphics.h>

CircleQueue mouse_queue;
CircleQueue keyboard_queue;
static boolean ctrl_status, shift_status, alt_status, caps_lock_status,
    ext_scancode;

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
static uint_8 ps2_read_byte(void)
{
    ps2_wait_output();
    return inb(PS2_DATA);
}

/**
 * Communicate with PS2 keyboard
 *****************************************************************************/
static uint_8 kbd_write(uint_8 data)
{
    ps2_wait_input();
    outb(PS2_DATA, data);
    ps2_wait_output();
    return inb(PS2_DATA);
}

static uint_8 mouse_write(uint_8 data)
{
    ps2_command_arg(MOUSE_WRITE, data);
    ps2_wait_output();
    return inb(PS2_DATA);
}

static int mouse_decode(struct mouse_raw_data *mdata, uint_8 code)
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
    char code = inb(PS2_DATA);
    int sucs = mouse_decode(&mrd, code);

    if (sucs) {
        struct queue_data qdata = {
            .data = mrd.cooked_mdata.y,
            /* .data = code */
        };
        draw_2d_gfx_cursor(mrd.cooked_mdata.x, mrd.cooked_mdata.y);

        /* ioqueue_put_data(&qdata, &mouse_queue); */
    }
    return;
}

/*
 * int 0x21;
 * Interrupt handler for Keyboard
 **/
void inthandler21(void)
{
    bool is_break_code;
    bool ctrl_pressed = ctrl_status;
    bool shift_pressed = shift_status;
    bool cap_lock_pressed = caps_lock_status;

    uint_16 scan_code = 0x0;
    
    while (inb(PS2_STATUS) & PS2_STR_OUTPUT_BUFFER_FULL) {
        scan_code = ps2_read_byte(); // get scan_code
    }
    if (scan_code == FLAG_EXT) {  // scan_code == 0xe0
        ext_scancode = true;
        return;
    }
    if (ext_scancode) {
        scan_code = 0xe000 | scan_code;
        ext_scancode = false;
    }

    is_break_code = (scan_code & 0x0080) != 0;
    if (is_break_code) {
        uint_16 make_code =
            (scan_code &= 0xff7f);  // break_code = 0x80+make_code
        if (make_code == MAKE_CTRL_R || make_code == MAKE_CTRL_L) {
            ctrl_status = false;
        } else if (make_code == MAKE_ALT_R || make_code == MAKE_ALT_L) {
            alt_status = false;
        } else if (make_code == MAKE_SHIFT_R || make_code == MAKE_SHIFT_L) {
            shift_status = false;
        } else {
        }
        return;
    } else if ((scan_code > 0x00 && scan_code < 0x3b) ||
               (scan_code == MAKE_ALT_R || scan_code == MAKE_CTRL_R)) {
        bool shift = false;

        /*   0x02 -> 1
         *   0x0d -> =
         *   0x0e -> backspace
         * */
        if ((scan_code >= 0x02 && scan_code < 0x0e) ||
            (scan_code == 0x1a) ||  // '['
            (scan_code == 0x1b) ||  // ']'
            (scan_code == 0x27) ||  // ';'
            (scan_code == 0x28) ||  // '\''
            (scan_code == 0x29) ||  // '`'
            (scan_code == 0x2b) ||  // '\\'
            (scan_code == 0x33) ||  // ','
            (scan_code == 0x34) ||  // '.'
            (scan_code == 0x35) /* '/'*/) {
            if (shift_pressed) {
                shift = true;
            }
        } else {  // alphabet
            if (shift_pressed && cap_lock_pressed) {
                shift = false;
            } else if (shift_pressed || cap_lock_pressed) {
                shift = true;
            } else {
                shift = false;
            }
        }

        uint_8 index = (scan_code & 0x00ff);
        char key = keymap[index][shift];
        if (key) {
            struct queue_data qdata = {
                .data = key,
            };
            ioqueue_put_data(&qdata, &keyboard_queue);

            return;
        }

        if (scan_code == MAKE_CTRL_L || scan_code == MAKE_CTRL_R) {
            ctrl_status = true;
        } else if (scan_code == MAKE_ALT_L || scan_code == MAKE_ALT_R) {
            alt_status = true;
        } else if (scan_code == MAKE_SHIFT_L || scan_code == MAKE_SHIFT_R) {
            shift_status = true;
        } else if (scan_code == MAKE_CAP_LOCK) {
            caps_lock_status = true;
        }

    } else {
        PAINC("unknow key");
    }
    return;
}

/**
 * Initialze i8042/AIP PS/2 controller.
 */
void ps2hid_init(void)
{
    // enable keyboard
    ps2_wait_input();
    outb(PS2_COMMAND, KBD_WRITE);

    ps2_wait_input();
    outb(PS2_DATA, KBDC_MODE);
    register_r0_intr_handler(INT_VECTOR_KEYBOARD, (Inthandle_t *) inthandler21);

    // enable ps2 mouse
    register_r0_intr_handler(INT_VECTOR_PS2_MOUSE,
                             (Inthandle_t *) inthandler2C);
    ps2_wait_input();
    outb(PS2_COMMAND, MOUSE_WRITE);
    ps2_wait_input();
    outb(PS2_DATA, MOUSE_ENABLE);
}
