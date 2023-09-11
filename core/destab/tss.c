#include <string.h>
#include <sys/descriptor.h>
#include <sys/tss.h>
#include <sys/threads.h>
#include <const.h>
#define K_STACK_ADDRESS 0x9f000

static TSS tss;

void create_tss_descriptor(Segment_Descriptor *des_addr,
                           uint_32 base,
                           uint_32 limit,
                           int dpl)
{
    create_descriptor(des_addr, base, limit,
                      DESC_P_1 | dpl | DESC_S_SYSTEM | DESC_TYPE_TSS,
                      DESC_G_4K | DESC_D_16 | DESC_L_32BITS | DESC_AVL);
}

void create_tss(void)
{
    uint_32 tss_size = sizeof(tss);  // - 4; // remove iomap
    memset(&tss, 0, tss_size);
    Selector sel_dat_ring0 = CREATE_SELECTOR(SEL_IDX_DATA_DPL_0, TI_GDT, RPL0);
    tss.ss0 = sel_dat_ring0;
    tss.esp0 = (uint_32 *) K_STACK_ADDRESS;
    tss.io_bitmaps_offset = tss_size;
    /* tss.iomap = 0xff; */

    Segment_Descriptor *gdt_start = get_gdt_base_address();
    Selector sel_TSS = CREATE_SELECTOR(SEL_IDX_TSS_DPL_0, TI_GDT, RPL0);
    create_tss_descriptor(gdt_start + SEL_IDX_TSS_DPL_0, (uint_32) &tss,
                          tss_size, DESC_DPL_0);
    __asm__ volatile("ltr %w0" ::"r"(sel_TSS));
}

void update_tss_esp0(TCB_t *thread){
    tss.esp0 = (uint_32*)((uint_32)thread + PG_SIZE);
}
