// head32 is for initing  arch-based code.
//
// in x86, like init IDT, GDT and TSS. add irq_headler to irq_headler_table

#include <asm/bootpack.h>
#include <asm/descriptor.h>
#include <frog/compiler.h>

extern void start_kernel(void);

__visible void __noreturn i386_start_kernel(void)
{
        init_gdt();
        init_idt();
        create_tss();

        start_kernel();
}
