#ifndef __SYS_TSS_H
#define __SYS_TSS_H
#include <ostype.h>

typedef struct tss {
    uint_16 prev_tss;
    uint_32 *esp0;
    uint_16 ss0;
    uint_32 *esp1;
    uint_16 ss1;
    uint_32 *esp2;
    uint_16 ss2;
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
    uint_16 es;
    uint_16 cs;
    uint_16 ss;
    uint_16 ds;
    uint_16 fs;
    uint_16 gs;
    uint_16 ldt_selector;
    uint_16 trace;   // for debug
    uint_16 io_bitmaps_offset;
}TSS;

#endif
