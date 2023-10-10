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
    //Slave chip
    //IRQ12 is PS/2 mouse
    //IRQ14 is AT disk
    _io_out8(PIC1_OCW1, PIC_OPEN_IRQ12 & PIC_OPEN_IRQ14);
    /* _io_out8(OCW1_S, PIC_MASK_ALL); */
    _io_delay();
}

static void frequency_set(uint_8 counter_port,\
                          uint_8 counter_no, \
                          uint_8 rwL,\
                          uint_8 counter_mode,\
                          uint_16 counter_value){
    _io_out8(PIT_CONTROL_PROT, (uint_8)(counter_no << 6 | rwL << 4 | counter_mode << 1));
    _io_out8(counter_port, (uint_8) counter_value);
    _io_out8(counter_port, (uint_8) counter_value >> 8);
}

void init_PIT8253(void)
{
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUTNER_MODE, COUNTER0_VALUE);
}
