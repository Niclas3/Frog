#ifndef BOOTPACK
#define BOOTPACK
#include <ostype.h>

// Function from core.s
// TODO: replace it with [asm] syntax
void _io_hlt(void);
void _io_delay(void);
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

void intr_exit(void);

#endif
