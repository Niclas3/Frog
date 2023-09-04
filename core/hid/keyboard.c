#include <hid/keyboard.h>
#include <hid/keymap.h>
#include <sys/pic.h>
#include <asm/bootpack.h>

#include <ioqueue.h>

CircleQueue keyboard_queue;

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
void inthandler21(){
    _io_out8(PIC0_OCW2, PIC_EOI_IRQ1);
    uint_8 scan_code =0x32;
    while (_io_in8(PORT_KEYSTATE) & KEYSTA_OUTPUT_BUFFER_FULL){
        scan_code = _io_in8(PORT_KEYDATE); // get scan_code
        struct queue_data qdata = { .data = scan_code, };
        ioqueue_put_data(&qdata, &keyboard_queue);
    }
    return;
}


