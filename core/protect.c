#include <asm/bootpack.h>
#include <protect.h>
#include <sys/descriptor.h>
#include <sys/int.h>
#include <sys/tss.h>

void register_ring0_INT(uint_32 int_vector_code, Inthandle_t handler_address)
{
    // 1.Get idt root address
    Descriptor_REG idtr_data = {0};
    save_idtr(&idtr_data);
    Gate_Descriptor *idt_start = (Gate_Descriptor *) idtr_data.address;
    Selector selector_code = CREATE_SELECTOR(SEL_IDX_CODE_DPL_0, TI_GDT, RPL0);
    create_gate(idt_start + int_vector_code, selector_code, handler_address,
                DESC_P_1 | DESC_DPL_0 | DESC_TYPE_INTR, 0);
    return;
}

/* Create some DPL = 0 descriptor base address 0x0 and limit is 0xffffffff */
void create_ring0_Descriptor(uint_32 desc_index, int des_type)
{
    Descriptor_REG gdtr_data = {0};
    save_gdtr(&gdtr_data);
    Segment_Descriptor *gdt_start = (Segment_Descriptor *) gdtr_data.address;
    create_descriptor(gdt_start + desc_index, 0x0, 0xffffffff,
                      DESC_P_1 | DESC_DPL_0 | DESC_S_DATA | des_type,
                      DESC_G_4K | DESC_D_32 | DESC_L_32BITS | DESC_AVL);
}

/* Create some DPL = 3 descriptor base address 0x0 and limit is 0xffffffff */
void create_ring3_Descriptor(uint_32 desc_index, int des_type)
{
    Descriptor_REG gdtr_data = {0};
    save_gdtr(&gdtr_data);
    Segment_Descriptor *gdt_start = (Segment_Descriptor *) gdtr_data.address;
    create_descriptor(gdt_start + desc_index, 0x0, 0xffffffff,
                      DESC_P_1 | DESC_DPL_3 | DESC_S_DATA | des_type,
                      DESC_G_4K | DESC_D_32 | DESC_L_32BITS | DESC_AVL);
}



void init_gdt(void)
{
    /* Descriptor_REG gdtr_data = {0}; */
    /* save_gdtr(&gdtr_data); */
    /* Selector sel_code = create_selector(SEL_IDX_CODE_DPL_0, TI_GDT, RPL0); */
    Selector sel_data = CREATE_SELECTOR(SEL_IDX_DATA_DPL_0, TI_GDT, RPL0);
    /* create_ring0_Descriptor(SEL_IDX_CODE_DPL_0, DESC_TYPE_CODEX); */
    /* create_ring0_Descriptor(SEL_IDX_DATA_DPL_0, DESC_TYPE_DATARW); */
    create_ring3_Descriptor(SEL_IDX_CODE_DPL_3, DESC_TYPE_CODEX);
    create_ring3_Descriptor(SEL_IDX_DATA_DPL_3, DESC_TYPE_DATARW);
    /*
     * gdtr_data.limit += 3 * 8;
     * load_gdtr(&gdtr_data);
     */
}

void init_idt(void)
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

void init_idt_gdt_tss(void){
    init_gdt();
    init_idt();
    create_tss(); // tss define in tss.c
}
