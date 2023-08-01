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
int_8  _io_in8(int port); // 1 byte
int_16 _io_in16(int port);// 2 bytes
int_32 _io_in32(int port);// 4 bytes
int _io_load_eflags(void);
void _io_store_eflags(int eflags);

void _asm_inthandler20(void); //Clock int
void _asm_inthandler21(void); //Keyboard int
void _asm_inthandler2C(void); //PS/2 mouse int

// Intel CPU Interrupt 0x0 ~ 0xF
void _divide_error();
void _single_step_exception();
void _nmi();
void _breakpoint_exception();
void _overflow();
void _bounds_check();
void _inval_opcode();
void _copr_not_available();
void _double_fault();
void _copr_seg_overrun();
void _inval_tss();
void _segment_not_present();
void _stack_exception();
void _general_protection();
void _page_fault();
void _copr_error();

// Define
typedef struct B_info {
    char cyls;
    char leds;
    char vmode;
    char reserve;
    short scrnx, scrny;
    unsigned char *vram;
} BOOTINFO;

typedef struct Color {
    unsigned char color_id;
} COLOR;
#endif
