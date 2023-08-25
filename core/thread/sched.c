#include <sys/sched.h>

#include <sys/pic.h>
#include <asm/bootpack.h>


/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20(void){
    _io_cli();
    _io_out8(PIC0_OCW2, PIC_EOI_IRQ0);
    static uint_8 switch_point = 0;
    /* if(switch_point == 0){ */
    /*     putfonts8_asc((char *)0xa0000, 320, 16, 15, COL8_0000FF, "Clock"); */
    /*     switch_point = 1; */
    /*     _io_sti(); */
    /* } else { */
    /*     putfonts8_asc((char *)0xa0000, 320, 16, 15, COL8_FFFFFF, "Clock"); */
    /*     switch_point = 0; */
    _io_sti();
    /* } */
    return;
}
