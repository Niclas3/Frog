// head32 is for initing  arch-based code.
//
// in x86, like init IDT, GDT and TSS. add irq_headler to irq_headler_table

#include <asm/bootpack.h>
#include <asm/descriptor.h>
#include <asm/i8253.h>   // PIT
#include <asm/i8259a.h>  // PIC
extern void start_kernel(void);
void i386_start_kernel()
{
        init_gdt();
        init_idt();
        create_tss();

        init_8259A();
        _io_sti();
        init_PIT8253();

        start_kernel();
}
