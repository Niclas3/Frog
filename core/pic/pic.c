#include <asm/bootpack.h>
#include <sys/pic.h>

void init_8259A(){
    //Set ICW1
    _io_out8(PIC0_ICW1, 0x11);
    _io_delay();
    _io_out8(PIC1_ICW1, 0x11);
    _io_delay();

    //Set ICW2
    _io_out8(PIC0_ICW2, 0x20);
    _io_delay();
    _io_out8(PIC1_ICW2, 0x28);
    _io_delay();

    //Set ICW3
    _io_out8(PIC0_ICW3, 0x04); // master IRQ2 links slave
    _io_delay();
    _io_out8(PIC1_ICW3, 0x02); // link with master IRQ2 
    _io_delay();

    //Set ICW4
    _io_out8(PIC0_ICW4, 0x01);
    _io_delay();
    _io_out8(PIC1_ICW4, 0x01);
    _io_delay();

    //Set OCW1
    //               IRQ0 is clock interrupt
    //               IRQ2 is link point 
    //               IRQ1 is keyboard interrupt
    
    /* _io_out8(PIC0_OCW1, PIC_OPEN_IRQ2 & PIC_OPEN_IRQ1); */
    _io_out8(PIC0_OCW1, PIC_OPEN_IRQ0 & PIC_OPEN_IRQ2 & PIC_OPEN_IRQ1);
    /* _io_out8(OCW1_M, PIC_OPEN_IRQ1); */
    _io_delay();
    //               IRQ12 is PS/2 mouse
    _io_out8(PIC1_OCW1, PIC_OPEN_IRQ12);
    /* _io_out8(OCW1_S, PIC_MASK_ALL); */
    _io_delay();
}
