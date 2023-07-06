#ifndef BOOTPACK
#define BOOTPACK
// Function from core.s
void _io_hlt(void);
/* void _print(char* msg, int len); */
/* void _print(void); */
void _write_mem8(int addr, int data);

void _io_cli(void);
void _io_out8(int port, int data);
int _io_load_eflags(void);
void _io_store_eflags(int eflags);

#endif
