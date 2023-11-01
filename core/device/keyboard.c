#include <hid/keyboard.h>
#include <hid/keymap.h>
#include <sys/pic.h>
#include <asm/bootpack.h>
#include <ostype.h>
#include <debug.h>

#include <ioqueue.h>

CircleQueue keyboard_queue;
static boolean ctrl_status, shift_status, alt_status, caps_lock_status, ext_scancode;

void wait_KBC_sendready(void){
    while(1){
        if((_io_in8(PORT_KEYSTATE) & KEYSTA_SEND_NOTREADY) == 0){
            break;
        }
    }
    return;
}

void init_keyboard(void){
    wait_KBC_sendready();
    _io_out8(PORT_KEYCOMMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    _io_out8(PORT_KEYDATE, KBC_MODE);
}


/*
 * int 0x21; 
 * Interrupt handler for Keyboard
 **/
void inthandler21(void){
    bool is_break_code;
    bool ctrl_pressed = ctrl_status;
    bool shift_pressed = shift_status;
    bool cap_lock_pressed = caps_lock_status;

    uint_16 scan_code =0x0;
    while (_io_in8(PORT_KEYSTATE) & KEYSTA_OUTPUT_BUFFER_FULL){
        scan_code = _io_in8(PORT_KEYDATE); // get scan_code
    }
    if(scan_code == FLAG_EXT){ // scan_code == 0xe0
        ext_scancode = true;
        return;
    }
    if(ext_scancode){
        scan_code = 0xe000 | scan_code;
        ext_scancode = false;
    }

    is_break_code = (scan_code & 0x0080 ) != 0;
    if(is_break_code){
        uint_16 make_code = (scan_code &= 0xff7f); //break_code = 0x80+make_code
        if(make_code == MAKE_CTRL_R || make_code == MAKE_CTRL_L){
            ctrl_status = false;
        } else if (make_code == MAKE_ALT_R || make_code == MAKE_ALT_L){
            alt_status = false;
        } else if (make_code == MAKE_SHIFT_R  || make_code == MAKE_SHIFT_L){
            shift_status = false;
        } else { }
        return;
    } else if((scan_code > 0x00 && scan_code < 0x3b) ||
              (scan_code == MAKE_ALT_R ||
               scan_code == MAKE_CTRL_R)){
        bool shift = false;

        /*   0x02 -> 1
         *   0x0d -> =
         *   0x0e -> backspace
         * */
        if((scan_code >= 0x02 && scan_code < 0x0e) ||
           (scan_code == 0x1a) ||  // '['
           (scan_code == 0x1b) ||  // ']'
           (scan_code == 0x27) ||  // ';'
           (scan_code == 0x28) ||  // '\''
           (scan_code == 0x29) ||  // '`'
           (scan_code == 0x2b) ||  // '\\'
           (scan_code == 0x33) ||  // ','
           (scan_code == 0x34) ||  // '.'
           (scan_code == 0x35)   /* '/'*/){

            if(shift_pressed){
                shift = true;
            }
        } else { // alphabet
            if (shift_pressed && cap_lock_pressed) {
                shift = false;
            } else if(shift_pressed || cap_lock_pressed){
                shift = true;
            } else {
                shift = false;
            }
        }

        uint_8 index = (scan_code & 0x00ff);
        char key = keymap[index][shift];
        if(key){
            struct queue_data qdata = { .data = key, };
            ioqueue_put_data(&qdata, &keyboard_queue);

            return;
        }

        if(scan_code == MAKE_CTRL_L || scan_code == MAKE_CTRL_R){
            ctrl_status = true;
        } else if(scan_code == MAKE_ALT_L || scan_code == MAKE_ALT_R){
            alt_status = true;
        } else if(scan_code == MAKE_SHIFT_L || scan_code == MAKE_SHIFT_R){
            shift_status = true;
        } else if (scan_code == MAKE_CAP_LOCK){
            caps_lock_status = true;
        }

    } else {
        PAINC("unknow key");
    }
    return;
}

