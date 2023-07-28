#include "../include/int.h"
#include "../include/graphic.h"
#include "../include/bootpack.h"
#include "../include/pic.h"

#include "../include/keyboard.h"
#include "../include/ps2mouse.h"

/*     Interrupt handler function Usage
 * !Check Init8259A setting interrupt is opened or not!
 *
 *   There are 3 function cooperating each other.
 *
 * Init8259A        : core.s
 * _asm_inthandler**: core.s
 * inthandler**     : interrupt/int.c 
 * create_gate      : destab/descriptor.c 
 *
 *   `_asm_inthandler**()` is the real callback function for interrupt,
 * according to interrupt callback function need to write in assemble
 * code, `_asm_inthandler**()` contains a C code to do some high level
 * things. This contained function is `inthandler**()`, you can add
 * function at high level C code for interrupt. 
 *   If you want to add a new interrupt handle code.First, you should add
 * assemble function at core.s and name it `_asm_inthandler**()`the `**`
 * represents hex number of this interrupt.  
 *   Then, you should call `create_gate()` to register this assemble
 * handler. Third, don't forget add a C code function at `interrupt/int.c`
 * as `inthandler**()`, and put it into `core.s:_asm_inthandler**()`
 *   Finally, you finish the interrupt setting. 
 **/


/*
 * int 0x21; 
 * Interrupt handler for Keyboard
 **/
void inthandler21(){
    int_8 scan_code = _io_in8(PORT_KEYDATE); // get scan_code
    putfonts8_asc((char *)0xa0000, 320, 8, 8, COL8_0000FF, scan_code);
    /* putfonts8_asc((char *)0xa0000, 320, 8, 8, COL8_0000FF, str); */
    return;
}

/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(){
    putfonts8_asc((char *)0xa0000, 320, 32, 32, COL8_FF00FF, "Clock");
    return;
}

/* int 0x2C;
 * Interrupt handler for PS/2 mouse
 **/
void inthandler2C(){
    _io_out8(PIC1_OCW2,0x64); // tell slave  IRQ12 is finish
    _io_out8(PIC0_OCW2,0x62); // tell master IRQ12 is finish
    char data = _io_in8(PORT_KEYDATE) ;
    putfonts8_asc((char *)0xa0000, 320, 16, 15, COL8_0000FF, "PS/2 Mouse");
    return;
}
