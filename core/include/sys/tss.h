#ifndef __SYS_TSS_H
#define __SYS_TSS_H
#include <ostype.h>
#include <sys/descriptor.h>

typedef struct thread_control_block TCB_t;

typedef struct tss {
    uint_32 prev_tss;
    uint_32 *esp0;
    uint_32 ss0;
    uint_32 *esp1;
    uint_32 ss1;
    uint_32 *esp2;
    uint_32 ss2;
    uint_32 cr3;
    uint_32 (*eip) (void);
    uint_32 eflags;
    uint_32 eax;
    uint_32 ecx;
    uint_32 edx;
    uint_32 ebx;
    uint_32 esp;
    uint_32 ebp;
    uint_32 esi;
    uint_32 edi;
    uint_32 es;
    uint_32 cs;
    uint_32 ss;
    uint_32 ds;
    uint_32 fs;
    uint_32 gs;
    uint_32 ldt_selector;
    uint_16 trace;   // for debug
    uint_16 io_bitmaps_offset;
    // uint_32 iomap;
}TSS;

void create_tss_descriptor(Segment_Descriptor *des_addr,
                           uint_32 base,
                           uint_32 limit,
                           int dpl);
void create_tss(void);

void update_tss_esp0(TCB_t *thread);
#endif
