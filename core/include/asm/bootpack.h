#ifndef BOOTPACK
#define BOOTPACK
#include <ostype.h>
#include <sys/descriptor.h>
// Function from core.s
void _io_hlt(void);
void _io_delay();
/* void _print(char* msg, int len); */
/* void _print(void); */
void _write_mem8(int addr, int data);

void _io_cli(void);
void _io_sti(void);
void _io_stihlt(void);
void _io_out8(int port, int data); // 1 byte
void _io_out16(int port, int data);// 2 bytes
void _io_out32(int port, int data);// 4 bytes
                                   //
uint_8  _io_in8(int port); // 1 byte
uint_16 _io_in16(int port);// 2 bytes
uint_32 _io_in32(int port);// 4 bytes
int _io_load_eflags(void);
void _io_store_eflags(int eflags);

void _asm_inthandler20(void); //Clock int
void _asm_inthandler21(void); //Keyboard int
void _asm_inthandler2C(void); //PS/2 mouse int

// Intel CPU Interrupt 0x0 ~ 0xF
void _divide_error(void);
void _single_step_exception(void);
void _nmi(void);
void _breakpoint_exception(void);
void _overflow(void);
void _bounds_check(void);
void _inval_opcode(void);
void _copr_not_available(void);
void _double_fault(void);
void _copr_seg_overrun(void);
void _inval_tss(void);
void _segment_not_present(void);
void _stack_exception(void);
void _general_protection(void);
void _page_fault(void);
void _copr_error(void);

#endif
