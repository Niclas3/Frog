#include <asm/bootpack.h>
#include <protect.h>
#include <sys/descriptor.h>
#include <sys/int.h>
#include <sys/tss.h>

uint_32 syscall_handler(void);
/**
 * All interrupt entry table is defined at core.s*/
extern void *intr_entry_table[IDT_DESC_CNT];
/*****************************************************************************/

/**
 *  All interrupt real handlers table
 *  register c function into this global table*/
Inthandle_t *intr_table[IDT_DESC_CNT];
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

/* Create some DPL = 0 descriptor base address 0x0 and limit is 0xffffffff */
static void create_ring0_Descriptor(uint_32 desc_index, int des_type)
{
    Descriptor_REG gdtr_data = {0};
    save_gdtr(&gdtr_data);
    Segment_Descriptor *gdt_start = (Segment_Descriptor *) gdtr_data.address;
    create_descriptor(gdt_start + desc_index, 0x0, 0xffffffff,
                      DESC_P_1 | DESC_DPL_0 | DESC_S_DATA | des_type,
                      DESC_G_4K | DESC_D_32 | DESC_L_32BITS | DESC_AVL);
}

/* Create some DPL = 1 descriptor base address 0x0 and limit is 0xffffffff */
static void create_ring1_Descriptor(uint_32 desc_index, int des_type)
{
    Descriptor_REG gdtr_data = {0};
    save_gdtr(&gdtr_data);
    Segment_Descriptor *gdt_start = (Segment_Descriptor *) gdtr_data.address;
    create_descriptor(gdt_start + desc_index, 0x0, 0xffffffff,
                      DESC_P_1 | DESC_DPL_1 | DESC_S_DATA | des_type,
                      DESC_G_4K | DESC_D_32 | DESC_L_32BITS | DESC_AVL);
}

/* Create some DPL = 3 descriptor base address 0x0 and limit is 0xffffffff */
static void create_ring3_Descriptor(uint_32 desc_index, int des_type)
{
    Descriptor_REG gdtr_data = {0};
    save_gdtr(&gdtr_data);
    Segment_Descriptor *gdt_start = (Segment_Descriptor *) gdtr_data.address;
    create_descriptor(gdt_start + desc_index, 0x00000000, 0xffffffff,
                      DESC_P_1 | DESC_DPL_3 | DESC_S_DATA | des_type,
                      DESC_G_4K | DESC_D_32 | DESC_L_32BITS | DESC_AVL);
}


static void init_gdt(void)
{
    // TODO: Move gdt init from loader.s to this file.

    /* Descriptor_REG gdtr_data = {0}; */
    /* save_gdtr(&gdtr_data); */
    /* Selector sel_code = create_selector(SEL_IDX_CODE_DPL_0, TI_GDT, RPL0); */
    Selector sel_data = CREATE_SELECTOR(SEL_IDX_DATA_DPL_0, TI_GDT, RPL0);
    /* create_ring0_Descriptor(SEL_IDX_CODE_DPL_0, DESC_TYPE_CODEX); */
    /* create_ring0_Descriptor(SEL_IDX_DATA_DPL_0, DESC_TYPE_DATARW); */
    create_ring3_Descriptor(SEL_IDX_CODE_DPL_3, DESC_TYPE_CODEX);
    create_ring3_Descriptor(SEL_IDX_DATA_DPL_3, DESC_TYPE_DATARW);

    create_ring1_Descriptor(SEL_IDX_CODE_DPL_1, DESC_TYPE_CODEX);
    create_ring1_Descriptor(SEL_IDX_DATA_DPL_1, DESC_TYPE_DATARW);
    /*
     * gdtr_data.limit += 3 * 8;
     * load_gdtr(&gdtr_data);
     */
}

static void init_idt(void)
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
     * all vector number are defined at <sys/int.h>
     *****************************************************************************/
    for (int vec_nr = 0; vec_nr < IDT_DESC_CNT; vec_nr++) {
        register_ring0_INT(vec_nr);
    }

    // System_call interrupt
    register_ring3_INT(INT_VECTOR_SYSCALL, syscall_handler);
}

//Public function
void init_idt_gdt_tss(void)
{
    init_gdt();
    init_idt();
    create_tss();  // tss define in tss.c
}

void register_r0_intr_handler(uint_32 int_vector_code, Inthandle_t handler){
    intr_table[int_vector_code] = handler;
    return;
}
