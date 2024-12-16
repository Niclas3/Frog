#include <kernel_print.h>
#include <stdio.h>
#include <string.h>
#include <sys/graphic.h>
#include <sys/pic.h>
#include <ostype.h>

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

//-----------------------------------------------------------------------------
//                     exception Callback function 
//-----------------------------------------------------------------------------
extern uint_32 g_boot_gfx_mode;
void exception_handler(int vec_no, int err_code, int eip, int cs, int eflags)
{
    char *err_msg[] = {
        "#DE Divide Error",
        "#DB RESERVED",
        "--  NMI Interrupt",
        "#BP Breakpoint",
        "#OF Overflow",
        "#BR BOUND Range Exceeded",
        "#UD Invalid Opcode (Undefined Opcode)",
        "#NM Device Not Available (No Math Coprocessor)",
        "#DF Double Fault",
        "    Coprocessor Segment Overrun (reserved)",
        "#TS Invalid TSS",
        "#NP Segment Not Present",
        "#SS Stack-Segment Fault",
        "#GP General Protection",
        "#PF Page Fault",
        "--  (Intel reserved. Do not use.)",
        "#MF x87 FPU Floating-Point Error (Math Fault)",
        /* "#AC Alignment Check", */
        /* "#MC Machine Check", */
        /* "#XF SIMD Floating-Point Exception" */
    };
    if (g_boot_gfx_mode == 1) {
        char str[50];
        /* sprintf(str, "No.%d, %s", vec_no, err_msg[vec_no]); */
        /* draw_info(0xc00a0000, 320, COL8_FF0000, 0, 100, str); */
        /* memset(str, 0, 50); */
        /* sprintf(str, "eip 0x%x ", eip); */
        /* draw_info(0xc00a0000, 320, COL8_FF0000, 0, 120, str); */
    } else if (g_boot_gfx_mode == 2) {
        kprint_with_cls("Exception\n");
        kprint("No.%d, %s\n", vec_no, err_msg[vec_no]);
        kprint("EIP 0x%x \n", eip);
    }
    return;
}
