#include "../include/descriptor.h"

void create_gate(Gate_Descriptor *gd,
                 Selector selector,
                 int_32 offset,
                 int attribute,
                 int_8 dcount)
{
    gd->offset_15_0 = offset & 0xffff;
    gd->selector = selector;
    gd->dw_count = dcount;
    gd->access_right = (attribute << 8) & 0xff00;
    gd->offset_32_16 = (offset >> 16) & 0xffff;
    return;
}

/* ; usage: Descriptor addressbase, limit, attr */
/* ;        Addressbase: dd */
/* ;        limit      : dd */
/* ;        attr       : dw */
/* %macro Descriptor 3 */
/*         dw %2 & 0xFFFF */
/*         dw %1 & 0xFFFF */
/*         db (%1 >> 16) & 0xFF */
/*         dw ((%2 >> 8) & 0xF00) | (%3 & 0xF0FF) */
/*         db (%1 >> 24) & 0xFF */
/* %endmacro */


/* Params:
 * int_32 limit;          20 bits=16+4 bits 
   int_8 attribute;        4 bits; G,B/D,L,AVL 
   int_8 access_right);    8 bits; p,DPL,S,TYPE 
 */
void create_descriptor(Segment_Descriptor *sd,
                       int_32 base_address,
                       int_32 limit,
                       int_8 access_right,//P, DPL ,S and type
                       int_8 attribute) //G, B/D, L, AVL,
{
    sd->base_address_15_00 = base_address & 0xffff;
    sd->limit_15_00= limit & 0xffff;
    sd->base_address_24_31 = base_address >> 24;
    sd->base_address_16_23 = (base_address >> 16) & 0xff;

    sd->access_right = access_right;
    sd->attribute_and_limit_16_19 = (attribute << 4) + ((limit >> 16) & 0xf); // limit 16~19
    return;
}

//  15                                  3  2  1     0
// ┌─────────────────────────────────────┬───┬──────┐
// │                                     │ T │ RPL  │
// │                Index                │ I │      │
// └─────────────────────────────────────┴───┴──────┘
//
// TI : Table Indicator
// RPL : 0 -> GDT
//       1 -> LDT
// Requested Privilege Level(RPL)
// 2 bytes
Selector create_selector(int_16 index, char ti, char rpl) 
{
    return (index << 3) + ti + rpl;
}

void create_interrupt_gate() {}
void create_trap_gate() {}
void create_task_gate() {}
