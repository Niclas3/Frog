#include "../include/pic.h"

#include "../include/bootpack.h"

void init_8259A(){
    //Set ICW1
    _io_out8(ICW1_M, 0x11);
    _io_delay();
    _io_out8(ICW1_S, 0x11);
    _io_delay();

    //Set ICW2
    _io_out8(ICW2_M, 0x20);
    _io_delay();
    _io_out8(ICW2_S, 0x28);
    _io_delay();

    //Set ICW3
    _io_out8(ICW3_M, 0x04); // master IRQ2 links slave
    _io_delay();
    _io_out8(ICW3_S, 0x02); // link with master IRQ2 
    _io_delay();

    //Set ICW4
    _io_out8(ICW4_M, 0x01);
    _io_delay();
    _io_out8(ICW4_S, 0x01);
    _io_delay();

    //Set OCW1
    //               IRQ2 is link point ,
    //               IRQ1 is keyboard interrupt
    _io_out8(OCW1_M, PIC_OPEN_IRQ2 & PIC_OPEN_IRQ1);
    /* _io_out8(OCW1_M, PIC_OPEN_IRQ1); */
    _io_delay();
    //               IRQ12 is PS/2 mouse
    _io_out8(OCW1_S, PIC_OPEN_IRQ12);
    /* _io_out8(OCW1_S, PIC_MASK_ALL); */
    _io_delay();
}
