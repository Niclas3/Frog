#ifndef PROTECT_H
#define PROTECT_H
#include <sys/descriptor.h>

void init_idt_gdt_tss(void);

void register_r0_intr_handler(uint_32 int_vector_code, Inthandle_t handler);
#endif
