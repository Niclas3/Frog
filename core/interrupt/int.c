#include <sys/int.h>
#include <sys/graphic.h>
#include <asm/bootpack.h>
#include <sys/pic.h>
#include <stdio.h>

#define IF_SET 0x00000200
#define GET_EFLAGS(EFLAG_V) __asm__ volatile ("pushfl; popl %0" : "=g" (EFLAG_V));

enum intr_status intr_get_status(void){
    uint_32 eflags = 0;
    GET_EFLAGS(eflags);
    return (eflags & IF_SET)? INTR_ON: INTR_OFF;
}
enum intr_status intr_set_status(enum intr_status status){
    return status & INTR_ON ? intr_enable() : intr_disable();
}
enum intr_status intr_enable(void){
    enum intr_status old_status;
    if(INTR_ON == intr_get_status()){
        old_status = INTR_ON;
        return old_status;
    }else{
        old_status = INTR_OFF;
        __asm__ volatile ("sti");
        return old_status;
    }
}
enum intr_status intr_disable(void){
    enum intr_status old_status;
    if(INTR_OFF == intr_get_status()){
        old_status = INTR_OFF;
        return old_status;
    }else{
        old_status = INTR_ON;
        __asm__ volatile ("cli":::"memory");
        return old_status;
    }
}

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

void exception_handler(int vec_no,int err_code,int eip,int cs,int eflags)
{
	char * err_msg[] = {"#DE Divide Error",
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

        char str[50];
        sprintf(str, "No.%d, %s", vec_no, err_msg[vec_no]);
        draw_info(0xc00a0000,320,COL8_FF0000,0,100, str);
        return;
    /* putfonts8_asc((char *)0xa0000, 320, 0, 0, COL8_FF0000, (unsigned char*)err_msg[vec_no]); */
}
