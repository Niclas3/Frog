#include <asm/bootpack.h>
#include <sys/descriptor.h>
#include <sys/int.h>
#include <protect.h>

void register_ring0_INT(uint_32 int_vector_code,Inthandle_t handler_address)
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

void init_ring0_Descriptor(uint_32 desc_index, int des_type)
{
    Descriptor_REG gdtr_data = {0};
    save_gdtr(&gdtr_data);

    Segment_Descriptor *gdt_start = (Segment_Descriptor *) gdtr_data.address;
    create_descriptor(gdt_start + desc_index, 0x0, 0xffffffff,
                      DESC_P_1 | DESC_DPL_0 | DESC_S_DATA | des_type,
                      DESC_G_4K | DESC_D_32 | DESC_L_32BITS | DESC_AVL);
}

void init_gdt()
{
    int desc_index = 0x1;
    Selector selector_code = create_selector(desc_index, TI_GDT, RPL0);
    init_ring0_Descriptor(desc_index, DESC_TYPE_CODEX);
}

void init_idt()
{
    register_ring0_INT(INT_VECTOR_DIVIDE, _divide_error);
    register_ring0_INT(INT_VECTOR_DEBUG, _single_step_exception);
    register_ring0_INT(INT_VECTOR_NMI, _nmi);
    register_ring0_INT(INT_VECTOR_BREAKPOINT, _breakpoint_exception);
    register_ring0_INT(INT_VECTOR_OVERFLOW, _overflow);
    register_ring0_INT(INT_VECTOR_BOUNDEX, _bounds_check);
    register_ring0_INT(INT_VECTOR_INVAL_OP, _inval_opcode);
    register_ring0_INT(INT_VECTOR_DEV_NOT_AVA, _copr_not_available);
    register_ring0_INT(INT_VECTOR_DOUBLE_FAULT, _double_fault);
    register_ring0_INT(INT_VECTOR_COP_SEG_OVERRUN, _copr_seg_overrun);
    register_ring0_INT(INT_VECTOR_INVAL_TSS, _inval_tss);
    register_ring0_INT(INT_VECTOR_SEG_NOT_PRESENT, _segment_not_present);
    register_ring0_INT(INT_VECTOR_STACK_FAULT, _stack_exception);
    register_ring0_INT(INT_VECTOR_PROTECTION, _general_protection);
    register_ring0_INT(INT_VECTOR_PAGE_FAULT, _page_fault);

    // Interrupt gate for IRQ0 aka clock interrupt
    register_ring0_INT(INT_VECTOR_INNER_CLOCK, _asm_inthandler20);
    // Interrupt gate for IRQ1 aka keyboard interrupt
    register_ring0_INT(INT_VECTOR_KEYBOARD, _asm_inthandler21);
    // Interrupt gate for IRQ12 aka PS/2 mouse interrupt
    register_ring0_INT(INT_VECTOR_PS2_MOUSE, _asm_inthandler2C);
}
