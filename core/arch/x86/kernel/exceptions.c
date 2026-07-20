#include <frog/types.h>
#include <frog/threads.h>
#include <kernel/debug.h>

/*     Interrupt handler function Usage
 * !Check Init8259A setting interrupt is opened or not!
 *
 *   There are 3 function cooperating each other.
 *
 * Init8259A        : core.s
 * _asm_inthandler**: core.s
 * inthandler**     : interrupt/int.c
 * create_gate      : destab/descriptor.c
 *
 *   `_asm_inthandler**()` is the real callback function for interrupt,
 * according to interrupt callback function need to write in assemble
 * code, `_asm_inthandler**()` contains a C code to do some high level
 * things. This contained function is `inthandler**()`, you can add
 * function at high level C code for interrupt.
 *   If you want to add a new interrupt handle code.First, you should add
 * assemble function at core.s and name it `_asm_inthandler**()`the `**`
 * represents hex number of this interrupt.
 *   Then, you should call `create_gate()` to register this assemble
 * handler. Third, don't forget add a C code function at `interrupt/int.c`
 * as `inthandler**()`, and put it into `core.s:_asm_inthandler**()`
 *   Finally, you finish the interrupt setting.
 **/


extern void page_fault_handler(uint_32 cr2, uint_32 err_code,
                               uint_32 eip, uint_32 cs,
                               const struct context_registers *context);
//-----------------------------------------------------------------------------
//                     exception Callback function 
//-----------------------------------------------------------------------------
void exception_handler(struct context_registers *context)
{
    if(context->vector_no == 0xe) {
            uint_32 cr2;
            __asm__ volatile ("mov %%cr2, %0": "=r"(cr2));
            page_fault_handler(cr2, context->err_code,
                               (uint_32) context->eip, context->cs,
                               context);
            return;
    } else {
            __asm__ volatile ("hlt;");
    }
}
