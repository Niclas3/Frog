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
                            // 0   1  2  3   bits
#define DESC_TYPE_TASK 0x5  // 0   1  0  1  (Task Gate)
#define DESC_TYPE_INTR 0xE  // D=1 1  1  0  (Interrupt Gate) D=1 for 386cpu else for 286
#define DESC_TYPE_TRAP 0xF  // D=1 1  1  1  (Trap Gate) D=1 for 386cpu else for 286
#define DESC_TYPE_CALL 0xC  // 1   1  0  0  (Call Gate)

// Bit 11 ~ 8
//                                   TYPE
// TYPE:  (4 bits) 
// TSS 
// `B` for busy
// The busy flag in the type field indicates whether the task is busy. A busy
// task currently running or suspended. A type field with a value of 1001B
// indicates an inactive task; a value of 1011B indicates a busy task. Task are
// not recursive. The processor uses the busy flag to detect an attempt to call
// a task whose execution has been interrupted.To ensure that there is noly one
// busy flag is associated with a task, each TSS should have only on TSS
// descriptor that points to it.
                            // 0  1  2    3   bits
#define DESC_TYPE_TSS 0x9   // 1  0  B=0  1   (TSS)

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
#define DESC_L_32BITS 0x0
#define DESC_L_64BITS 0x2

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
    uint_16 limit_15_00;
    uint_16 base_address_15_00;
    uint_8 base_address_16_23;
    uint_8 access_right;              //P, DPL ,S and type.
    uint_8 attribute_and_limit_16_19; //G, B/D, L, AVL, and limit16-19
    uint_8 base_address_24_31;
} Segment_Descriptor;

// GDTR layout
//
// 47                                 15              0
// ┌─────────────────────────────────┬────────────────┐
// │                                 │                │
// │        GDT base address         │    GDT Limit   │
// │                                 │                │
// └─────────────────────────────────┴────────────────┘
typedef struct _Descriptor_register_layout{
    uint_16 limit;
    uint_32 address;
} Descriptor_REG;

// Get gdtr or idtr data 
void save_gdtr(Descriptor_REG *data);
void load_gdtr(Descriptor_REG *data);

// Set gdtr or idtr data
void save_idtr(Descriptor_REG *data);
void load_idtr(Descriptor_REG *data);

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
    uint_16 offset_15_0;  // 2 bytes
    uint_16 selector;     // function address called
    uint_8 dw_count;      // param.count 1 byte, 8 bits, at call-gate
    uint_8 access_right;  // P + DPL + type  (101110)
    uint_16 offset_32_16;
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
//
#define TI_GDT 0x0
#define TI_LDT 0x4
#define RPL0 0x0
#define RPL1 0x1
#define RPL2 0x2
#define RPL3 0x3
typedef short Selector;

//Preset descriptor's selectors
//Init at protect.c
#define SEL_IDX_CODE_DPL_0   1
#define SEL_IDX_DATA_DPL_0   2
#define SEL_IDX_VIDEO_DPL_0  3
#define SEL_IDX_VGC_DPL_0    4
#define SEL_IDX_TSS_DPL_0    5
#define SEL_IDX_CODE_DPL_3   6
#define SEL_IDX_DATA_DPL_3   7

#define CREATE_SELECTOR(IDX, TI, RPL) (IDX << 3) + TI + RPL

//==============================================================================
typedef void* (Inthandle_t)(void*);

void create_gate(Gate_Descriptor *gd,
                 Selector selector,
                 Inthandle_t offset, // funtion
                 uint_8 attribute,
                 uint_8 dcount);

void create_descriptor(Segment_Descriptor *sd,
                       uint_32 base_address,
                       uint_32 limit,  //   20 bits=16+4 bits
                       uint_8 access_right,//8 bits; P,DPL,S,TYPE
                       uint_8 attribute); //   4 bits; G,B/D,L,AVL

Segment_Descriptor* get_idt_base_address(void);
Segment_Descriptor* get_gdt_base_address(void);

#endif
