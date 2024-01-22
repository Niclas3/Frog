#ifndef __LIB_CONST_H
#define __LIB_CONST_H

#define HZ 9000

#define PG_SIZE 4096U
//   0|CF (Carry flag): This bit is set by arithmetic instructions that generate either a carry or a borrow. This bit can also be set, cleared, or inverted with the STC, CLC, or CMC instructions, respectively Carry flag is also used in shift and rotate instructions to contain the bit shifted or rotated out of the register.
//   1|1 
//   2|PF (Parity flag): The parity bit is set by most instructions if the least significant 8-bit of the result contains an even number of one’s
//   3|0
//   4|AF (Auxiliary carry flag): This bit is set when there is a carry or borrow after a nibble addition or subtraction, respectively. The programmer can’t access this bit directly, but this bit is internally used for BCD arithmetic.
//   5|0
//   6|ZF (Zero flag): Zero flags is set to 1 if the result of an operation is zero
//   7|SF (Sign flag): The signed numbers are represented by a combination of sign and magnitude The Most Significant Bit (MSB) indicates a sign of the number. For a negative number, MSB is 1. The sign flag is set to 1 if the result of an operation is negative (MSB=1).
//   8|TF (Trap Flag): Trap flag allows users to single-step through programs. When an 80386 detects that this flag is set, it executes one instruction and then automatically generates an internal exception 1. After servicing the exception, the processor executes the next instruction and repeats the process. This single stepping continues until the program code resets this flag for debugging programs’ single-step facility is used.
//   9|IF (Interrupt Flag): When the interrupt flag is set, the 80386 recognizes and handles external hardware interrupts on its INTR pin. If the interrupt flag is cleared, 80386 ignores any inputs on this pin. The IF flag is set and cleared with the STI and CLI instructions, respectively.
//   A|DF (Direction flag): The direction flag controls the direction of string operations. When the D flag is cleared these operations process strings from low memory up to high memory. This means that offset pointers (usually SI and DI) are incremented by 1 after each operation in the string instructions when the D flag is cleared. If the D flag is set, then SI and DI are decremented by 1 after each operation to process strings from high to low memory
//   B|OF (Overflow flag): In 2’s complemented arithmetic, the most significant bit is used to represent a sign, and the remaining bits are used to represent the magnitude of a number. This flag is set if the result of a signed operation is too large to fit in the number of bits available (7-bits for an 8-bit number) to represent it.
//   C+IOPL (VO Privilege level): The two bits in the IOPL are used by the processor and the operating system to determine your application’s access to I/O facilities. It holds a privilege level, from 0 to 3, at which the current code is running in order to execute any I/O-related instruction.
//   D+
//   E|NT (Nested flag): This flag is set when one system task invokes another task
//   F|0
//  10|R (Resume) flag/Restart flag: This flag, when set allows selective masking of some exceptions at the time of debugging
//  11|1. VM (Virtual Memory) flag: This flag indicates the operating mode of 80386. When the VM flag is set, 80386 switches from protected mode to virtual 8086 modes.
//  12|AC Alignment Check
//  13|VIF Virtual Interrupt Flag
//  14|VIP Virtual Interrupt Pending
//  15|ID  ID Flag
// H -> L
// EFLAGS
//+--+---+---+--+--+--+--+--+----+--+--+--+--+--+--+--+--+--+--+--+--+
//|15|14 |13 |12|11|10|f |e |d  c|b |a |9 |8 |7 |6 |5 |4 |3 |2 |1 |0 |
//+--+---+---+--+--+--+--+--+----+--+--+--+--+--+--+--+--+--+--+--+--+
//|id|vip|vif|ac|vm|rf|0 |nt|IOPL|of|df|if|tf|sf|zf|0 |af|0 |pf|0 |cf|
//+--+---+---+--+--+--+--+--+----+--+--+--+--+--+--+--+--+--+--+--+--+
#define EFLAGS_RESERVED (1 << 1)
#define EFLAGS_IF_1 (1 << 9)
#define EFLAGS_IF_0 0
#define EFLAGS_IOPL_3 (3 << 12)
#define EFLAGS_IOPL_0 (0 << 12)

// TASK number aka pid

#define TASK_KERNEL 1
#define TASK_SYS 2
#define TASK_FS 3

#ifndef NULL
#define NULL ((void *) 0)
#endif

#endif

