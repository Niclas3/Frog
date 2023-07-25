#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H
#include "ostype.h"
//Start at 0 
//==============================================================================
//Segment Descriptor Attributes                                                |
//==============================================================================
// Bit 11 ~ 8
//                                   TYPE
// TYPE:  (4 bits) 
// NON-SYSTEM SEGMENTS
// `A` stands for access, it should be set 0 when it be created.
// `C` stands for conforming
// `*` set 0 when creating new descriptor
// calculate on A = 0 , 
// Code segment
                               // 0  1  2  3   bits
                               // X  C  R  A
#define DESC_TYPE_CODEX   0x8  // 1  0  0  *  (code only execute)
#define DESC_TYPE_CODEXR  0xA  // 1  0  1  *  (code execute and readable)
#define DESC_TYPE_CODEXC  0xC  // 1  1  0  *  (code execute and conforming)
#define DESC_TYPE_CODEXCR 0xE  // 1  1  1  *  (code execute readable and conforming)
// Data segment
                               // 0  1  2  3   bits
                               // X  E  W  A
#define DESC_TYPE_DATAR   0x0  // 0  0  0  *  (data only readable)
#define DESC_TYPE_DATARW  0x2  // 0  0  1  *  (data readable and writable)
#define DESC_TYPE_DATARE  0x4  // 0  1  0  *  (data readable descend to expand)
#define DESC_TYPE_DATARWE 0x6  // 0  1  1  *  (data readable and writeable and descend to expand)
                               //
// Bit 11 ~ 8
//                                   TYPE
// TYPE:  (4 bits) 
// SYSTEM SEGMENTS
// `D` stands for size of gate, 1 = 32 bits; 0 = 16 bits
//
// * Value calculate on D=1
// Task Gate  
                            // 0  1  2  3   bits
#define DESC_TYPE_TASK 0x5  // 0  1  0  1  (Task Gate)
#define DESC_TYPE_INTR 0x6  // D  1  1  0  (Interrupt Gate) D=1 for 386cpu else for 286
#define DESC_TYPE_TRAP 0x7  // D  1  1  1  (Trap Gate) D=1 for 386cpu else for 286
#define DESC_TYPE_CALL 0xC  // 1  1  0  0  (Call Gate)

// Bit 12
// S: if or not if system segment
// 000* 0000 0000 0000
#define DESC_S_SYSTEM 0x00
#define DESC_S_DATA   0x10

// Bit 14~13
// Descriptor Privilege Level 
// 0**0 0000 0000 0000
#define DESC_DPL_0 0x00
#define DESC_DPL_1 0x20
#define DESC_DPL_2 0x40
#define DESC_DPL_3 0x60

// Bit 15
// Present
// segment if present or not
// *000 0000 0000 0000
#define DESC_P_1 0x80
#define DESC_P_0 0x00

// Bit 20
// AVL: Available to Sys. Programmers -- 
// CPU does not use it. os can define it by itself.
// 000* 0000 0000 0000 0000 0000
#define DESC_AVL 0x0

// Bit 21
// L : stand for 64-bits system
// 1 for yes
// 0 for no
// 00*0 0000 0000 0000 0000 0000
#define DESC_L_32bit 0x0
#define DESC_L_64bit 0x2

// Bit 22
// D/B
// For Code-segment descriptor bit 22 is D, it stands for `Default`
// if `D` is 0, CPU uses `IP`  register for code. 16 bits code
// if `D` is 1, CPU uses `EIP` register for code. 32 bits code
//-----------------------------------------------------------------
// For Data-segment descriptor bit 22 is B, it stands for `Big`
// if `B` is 0, CPU uses `SP`  register for stack. 16 bits
// if `B` is 1, CPU uses `ESP` register for stack. 32 bits
// 0*00 0000 0000 0000 0000 0000
#define DESC_D_16  0x0
#define DESC_D_32  0x4

#define DESC_B_16  0x0
#define DESC_B_32  0x4

// G flag -- Bit 23 in the second doubleword of a segment descriptor
// if G is 0 then real_limit = (limit + 1) * 1 -1
// if G is 1 then real_limit = (limit + 1) * 4kb -1
// *000 0000 0000 0000 0000 0000
#define DESC_G_4K 0x8
#define DESC_G_1B 0x0

typedef struct _Descriptor {
    int_16 limit_15_00;
    int_16 base_address_15_00;
    int_8 base_address_16_23;
    int_8 access_right;              //P, DPL ,S and type.
    int_8 attribute_and_limit_16_19; //G, B/D, L, AVL, and limit16-19
    int_8 base_address_24_31;
} Segment_Descriptor;

//==============================================================================
// Gate Attributes (gate is a system descriptor)

// Interrupt Gate
// high 32-bits
//  -------------------------------------------------------
//  |                     | S | type  |                    |
//  -------------------------------------------------------
//  |31 - 16 | 15 | 14 13 | 12  -   8 | 7  -  5 | 4  -  0 |
//  |offset  | P  |  DPL  | 0 D 1 1 0 | 0  0  0 | Reserve |
// low 32-bits
//  |seg sel |              offset 15..0                  |
//  -------------------------------------------------------
// D size of gate 1=32bits; 0=16bits
// P segment present flag
// DPL descriptor privilege level
// low to high up to down
typedef struct _GATE {
    int_16 offset_15_0;  // 2 bytes
    int_16 selector;     // function address called
    int_8 dw_count;      // param.count 1 byte, 8 bits, at call-gate
    int_8 access_right;  // P + DPL + type  (101110)
    int_16 offset_32_16;
} Gate_Descriptor;
//==============================================================================


//Segment Selector
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
typedef short Selector;
//==============================================================================

void create_gate(Gate_Descriptor *gd,
                 Selector selector,
                 int_32 offset,
                 int attribute,
                 int_8 dcount);

void create_descriptor(Segment_Descriptor *sd,
                       int_32 base_address,
                       int_32 limit,  //   20 bits=16+4 bits
                       int_8 access_right,//8 bits; P,DPL,S,TYPE
                       int_8 attribute); //   4 bits; G,B/D,L,AVL

Selector create_selector(int_16 index, char ti, char rpl);

void create_interrupt_gate();
void create_trap_gate();
void create_task_gate();


#endif
