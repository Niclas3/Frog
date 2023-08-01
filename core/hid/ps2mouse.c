#include <hid/ps2mouse.h>
#include <asm/bootpack.h>
#include <hid/keyboard.h>


void enable_mouse(){
    wait_KBC_sendready();
    _io_out8(PORT_KEYCOMMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    _io_out8(PORT_KEYDATE, MOUSECMD_ENABLE);

    return;
}
