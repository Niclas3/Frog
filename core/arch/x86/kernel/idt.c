#include <asm/descriptor.h>
#include <frog/types.h>
#include <asm/int.h>

// syscall_handler defined in core.s
uint_32 syscall_handler(void);
/**
 * All interrupt entry table is defined at core.s
 * hardware interrupt
 * */
extern void *intr_entry_table[IDT_DESC_CNT];
/*****************************************************************************/


/*****************************************************************************/
static void register_INT(uint_32 int_vector_code,
                         Inthandle_t handler_address,
                         uint_32 dpl)
{
    // 1.Get idt root address
    Descriptor_REG idtr_data = {0};
    save_idtr(&idtr_data);
    Gate_Descriptor *idt_start = (Gate_Descriptor *) idtr_data.address;
    Selector selector_code = CREATE_SELECTOR(SEL_IDX_CODE_DPL_0, TI_GDT, RPL0);
    create_gate(idt_start + int_vector_code, selector_code, handler_address,
                DESC_P_1 | dpl | DESC_TYPE_INTR, 0);
    return;
}

void register_ring0_INT(uint_32 int_vector_code)
{
    Inthandle_t *handler = intr_entry_table[int_vector_code];
    register_INT(int_vector_code, handler, DESC_DPL_0);
    return;
}

static void register_ring3_INT(uint_32 int_vector_code, Inthandle_t handler_address)
{
    register_INT(int_vector_code, handler_address, DESC_DPL_3);
    return;
}


void init_idt(void)
{
    // 1.Get idt root address
    Descriptor_REG idtr_data = {0};
    save_idtr(&idtr_data);
    // 2. move base address to high 1G memory
    idtr_data.address |= 0xc0000000;
    load_idtr(&idtr_data);
    /**
     * Register all exception and all outer interrupt
     * IDT_DESC_CNT is 0x30
     * all vector number are defined at <asm/int.h>
     * ONLY INIT first 0x30 interrupts for CPU request
     *****************************************************************************/
    for (int vec_nr = 0; vec_nr < IDT_DESC_CNT; vec_nr++) {
        register_ring0_INT(vec_nr);
    }

    // System_call interrupt
    register_ring3_INT(INT_VECTOR_SYSCALL, syscall_handler);
}

