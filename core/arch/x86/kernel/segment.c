#include <asm/descriptor.h>
#include <frog/types.h>

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


void init_gdt(void)
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
