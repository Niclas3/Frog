#ifndef PROTECT_H
#define PROTECT_H
#include <sys/descriptor.h>

void init_idt_gdt_tss(void);

void register_ring0_INT(uint_32 int_vector_code, Inthandle_t handler_address);
#endif
