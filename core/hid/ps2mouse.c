#include <hid/ps2mouse.h>
#include <asm/bootpack.h>
#include <hid/keyboard.h>

#include <global.h>

#include <sys/pic.h>

struct MOUBUF mousebuf;

void enable_mouse(){
    wait_KBC_sendready();
    _io_out8(PORT_KEYCOMMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    _io_out8(PORT_KEYDATE, MOUSECMD_ENABLE);

    return;
}

/* int 0x2C;
 * Interrupt handler for PS/2 mouse
 **/
void inthandler2C(void){
    _io_out8(PIC1_OCW2, PIC_EOI_IRQ12); // tell slave  IRQ12 is finish
    _io_out8(PIC0_OCW2, PIC_EOI_IRQ2); // tell master IRQ2 is finish
    char data = _io_in8(PORT_KEYDATE) ;
    /* draw_hex(0xa0000,320,COL8_0000FF,16,15,data); */
    /* putfonts8_asc((char *)0xa0000, 320, 16, 15, COL8_0000FF, "PS/2 Mouse"); */

    return;
}

