#include "include/protect.h"
#include "include/bootpack.h"
#include "include/descriptor.h"
#include "include/int.h"


void init_ring0_INT(int_32 int_vector_code, int_32 handler_address)
{
    // 1.Get idt root address
    Descriptor_REG idtr_data = {0};
    save_idtr(&idtr_data);
    Gate_Descriptor *idt_start = (Gate_Descriptor *) idtr_data.address;
    Selector selector_code = create_selector(1, TI_GDT, RPL0);
    create_gate(idt_start + int_vector_code, selector_code, handler_address,
                DESC_P_1 | DESC_DPL_0 | DESC_TYPE_INTR, 0);
    return;
}

void init_idt()
{
    init_ring0_INT(INT_VECTOR_DIVIDE, _divide_error);
    init_ring0_INT(INT_VECTOR_DEBUG, _single_step_exception);
    init_ring0_INT(INT_VECTOR_NMI, _nmi);
    init_ring0_INT(INT_VECTOR_BREAKPOINT, _breakpoint_exception);
    init_ring0_INT(INT_VECTOR_OVERFLOW, _overflow);
    init_ring0_INT(INT_VECTOR_BOUNDEX, _bounds_check);
    init_ring0_INT(INT_VECTOR_INVAL_OP, _inval_opcode);
    init_ring0_INT(INT_VECTOR_DEV_NOT_AVA, _copr_not_available);
    init_ring0_INT(INT_VECTOR_DOUBLE_FAULT, _double_fault);
    init_ring0_INT(INT_VECTOR_COP_SEG_OVERRUN, _copr_seg_overrun);
    init_ring0_INT(INT_VECTOR_INVAL_TSS, _inval_tss);
    init_ring0_INT(INT_VECTOR_SEG_NOT_PRESENT, _segment_not_present);
    init_ring0_INT(INT_VECTOR_STACK_FAULT, _stack_exception);
    init_ring0_INT(INT_VECTOR_PROTECTION, _general_protection);
    init_ring0_INT(INT_VECTOR_PAGE_FAULT, _page_fault);

    // Interrupt gate for IRQ1 aka keyboard interrupt
    init_ring0_INT(INT_VECTOR_KEYBOARD, _asm_inthandler21);
    // Interrupt gate for IRQ12 aka PS/2 mouse interrupt
    init_ring0_INT(INT_VECTOR_PS2_MOUSE, _asm_inthandler2C);
}