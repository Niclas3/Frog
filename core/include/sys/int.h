#ifndef __SYS_INT_H
#define __SYS_INT_H

// Interrupt 0 -- Divide Error Exception (#DE) (fault)
#define INT_VECTOR_DIVIDE 0x0
// Interrupt 1 -- Debug Exception (#DB) (Trap or fault)
#define INT_VECTOR_DEBUG 0x1
// Interrupt 2 -- NMI Interrupt (NMI) (Not applicable)
#define INT_VECTOR_NMI 0x2
// Interrupt 3 -- Breakpoint Exception (#BP) (Trap)
#define INT_VECTOR_BREAKPOINT 0x3
// Interrupt 4 -- Overflow Exception (#OF) (Trap)
#define INT_VECTOR_OVERFLOW 0x4
// Interrupt 5 -- BOUND Range Exceeded Exception (#BR) (fault)
#define INT_VECTOR_BOUNDEX 0x5
// Interrupt 6 -- Invalid Opcode Exception (#UD) (falut)
#define INT_VECTOR_INVAL_OP 0x6
// Interrupt 7 -- Device Not Available Exception (#NM) (fault)
#define INT_VECTOR_DEV_NOT_AVA 0x7
// Interrupt 8 -- Double Fault Exception (#DF) (abort)
#define INT_VECTOR_DOUBLE_FAULT 0x8
// Interrupt 9 -- Coprocessor Segment Overrun (abort)
#define INT_VECTOR_COP_SEG_OVERRUN 0x9
// Interrupt 10 --Invalid TSS Exception (#TS) (fault)
#define INT_VECTOR_INVAL_TSS 0xA
// Interrupt 11 --Segment Not Present (#NP) (fault)
#define INT_VECTOR_SEG_NOT_PRESENT 0xB
// Interrupt 12 --Stack Fault Exception (#SS) (fault)
#define INT_VECTOR_STACK_FAULT 0xC
// Interrupt 13 --General Protection Exception (#GP) (fault)
#define INT_VECTOR_PROTECTION 0xD
// Interrupt 14 --Page-Fault Exception (#PF) (fault)
#define INT_VECTOR_PAGE_FAULT 0xE

// Interrupt 16 --x87 FPU Floating-Point Error(#MF) (fault)
#define INT_VECTOR_X87_FPU_ERROR 0x10
// Interrupt 17 --Alignment Check Exception (#AC) (fault)
#define INT_VECTOR_ALIGN_CHECK   0x11
// Interrupt 18 --Machine-Check Exception (#MC) (abort)
#define INT_VECTOR_MACHINE_CHECK 0x12
// Interrupt 19 --SIMD Floating-Point Exception (#XM) (falut)
#define INT_VECTOR_SIMD_FP 0x13
// Interrupt 20 --Virtualization Exception (#VE) (falut)
#define INT_VECTOR_VIRTUALIZATION 0x14
// Interrupt 21 --Control Protection Exception (#CP) (falut)
#define INT_VECTOR_CTRL_PROCTECTION 0x15

/* Interrupt 32 to 255 --- User Defined Interruptes */
#define INT_VECTOR_KEYBOARD     0x21
#define INT_VECTOR_PS2_MOUSE    0x2C
#define INT_VECTOR_INNER_CLOCK  0x20

//-----------------------------------------------------------------------------
//                     Interrupt control
//-----------------------------------------------------------------------------
enum intr_status {
    INTR_OFF,
    INTR_ON
};

enum intr_status intr_get_status(void);
enum intr_status intr_set_status(enum intr_status status);
enum intr_status intr_enable(void);
enum intr_status intr_disable(void);

//-----------------------------------------------------------------------------
//                     Interrupt Callback function 
//-----------------------------------------------------------------------------
void exception_handler(int vec_no,int err_code,int eip,int cs,int eflags);
#endif
