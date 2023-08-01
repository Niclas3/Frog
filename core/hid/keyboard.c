#include <hid/keyboard.h>
#include <hid/keymap.h>
/* #include "../include/bootpack.h" */

void wait_KBC_sendready(){
    while(1){
        if((_io_in8(PORT_KEYSTATE) & KEYSTA_SEND_NOTREADY) == 0){
            break;
        }
    }
    return;
}

void init_keyboard(){
    wait_KBC_sendready();
    _io_out8(PORT_KEYCOMMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    _io_out8(PORT_KEYDATE, KBC_MODE);
}
