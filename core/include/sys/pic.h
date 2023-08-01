#ifndef PIC_H
#define PIC_H

/**
 *                ┌────────┐                      ┌────────┐
 *     │          │        │◄───── IRQ0 Clock     │        │◄────── IRQ8 Real clock 
 *     │          │        │                      │        │
 *     ▼NMI       │        │◄───── IRQ1 Keyboard  │        │◄────── IRQ9 Redirection IRQ2 
 * ┌───────┐      │        │                      │        │
 * │       │   INT│ Master │◄─────────────────────┤ Slave  │◄────── IRQ10 Reserve 
 * │  CPU  │◄─────┤        │                      │        │
 * │       │INTR  │        │◄───── IRQ3 Serial2   │        │◄────── IRQ11 Reserve 
 * └───────┘      │        │                      │        │
 *                │        │◄───── IRQ4 Serial1   │        │◄────── IRQ12 PS/2 mouse 
 *                │        │                      │        │
 *                │        │◄───── IRQ5 LPT2      │        │◄────── IRQ13 FPU exception 
 *                │        │                      │        │
 *                │        │◄───── IRQ6 Floppy    │        │◄────── IRQ14 AT disk 
 *                │        │                      │        │
 *                │        │◄───── IRQ7 LPT1      │        │◄────── IRQ15 Reserve 
 *                └────────┘                      └────────┘
 **/

// Four register layouts(ICW1~ICW4)
//  ICW1
/* ┌─────┐ 
 * │  7  │ ─┐ 
 * ├─────┤  │ 
 * │  6  │  ├─ for PC must be 0 
 * ├─────┤  │ 
 * │  5  │ ─┘ 
 * ├─────┤ 
 * │  4  │ 1=for ICW1 must 1(ports is 0x20 or 0xA0) 
 * ├─────┤ 
 * │  3  │ 1=level triggered mode, 0=edge triggered mode 
 * ├─────┤ 
 * │  2  │ 1=4bytes int vector, 0=8bytes int vector 
 * ├─────┤
 * │  1  │ 1=single 8259, 0=cascade 8259 
 * ├─────┤ 
 * │  0  │ 1=need ICW4, 0=no need ICW4 
 * └─────┘ 
 *  ICW1    M    S 
 * (ports 0x20,0xA0)
 */

//  ICW2 
/* ┌─────┐ 
 * │  7  │ ─┐                  
 * ├─────┤  │ 
 * │  6  │  │
 * ├─────┤  │
 * │  5  │  ├─ 80x86 int vector
 * ├─────┤  │
 * │  4  │  │
 * ├─────┤  │
 * │  3  │ ─┘
 * ├─────┤ 
 * │  2  │ ─┐ 
 * ├─────┤  │ 
 * │  1  │  ├─ 000:80x86 system
 * ├─────┤  │
 * │  0  │ ─┘ 
 * └─────┘                      
 *  ICW2    M    S
 * (ports 0x21,0xA1)
 */

//               port                                  
//  ICW3 master (0x21)                     | ICW3 slave (0xA1)                                      
/* ┌─────┐                                 |┌─────┐ 
 * │  7  │ 1=IR7 cascade slave, 0=no slave |│  7  │ ─┐                  
 * ├─────┤                                 |├─────┤  │ 
 * │  6  │ 1=IR6 cascade slave, 0=no slave |│  6  │  │
 * ├─────┤                                 |├─────┤  │
 * │  5  │ 1=IR5 cascade slave, 0=no slave |│  5  │  ├─ must be 0
 * ├─────┤                                 |├─────┤  │
 * │  4  │ 1=IR4 cascade slave, 0=no slave |│  4  │  │
 * ├─────┤                                 |├─────┤  │
 * │  3  │ 1=IR3 cascade slave, 0=no slave |│  3  │ ─┘
 * ├─────┤                                 |├─────┤ 
 * │  2  │ 1=IR2 cascade slave, 0=no slave |│  2  │ ─┐
 * ├─────┤                                 |├─────┤  │
 * │  1  │ 1=IR1 cascade slave, 0=no slave |│  1  │  ├─ IR number of master which connects slave chip
 * ├─────┤                                 |├─────┤  │
 * │  0  │ 1=IR0 cascade slave, 0=no slave |│  0  │ ─┘ 
 * └─────┘                                 |└─────┘                       
 *  ICW3 master                            | ICW3 slave 
 * (ports 0x21)                              (0xA1)
 * 
 */

//  ICW4 (0x21h, 0xA1h)
/* ┌─────┐ 
 * │  7  │ ─┐                  
 * ├─────┤  │ 
 * │  6  │  ├─ unused(set to 0)
 * ├─────┤  │
 * │  5  │ ─┘
 * ├─────┤
 * │  4  │ 1=SFNM mode, 0=sequential mode
 * ├─────┤
 * │  3  │ ─┐
 * ├─────┤  ├─  master/slave buffer mode
 * │  2  │ ─┘
 * ├─────┤
 * │  1  │ 1=auto EOI, 0=normal EOI
 * ├─────┤ 
 * │  0  │ 1=80x86mode, 0=MCS 80/85
 * └─────┘                      
 *  ICW4    M    S
 * (ports 0x21,0xA1)
 * EOI:End Of Interrupt
 */
// TWO OCW1 OCW2

/* ┌─────┐ 
 * │  7  │ 0=IRQ7 open, 1=close 
 * ├─────┤ 
 * │  6  │ 0=IRQ6 open, 1=close
 * ├─────┤
 * │  5  │ 0=IRQ5 open, 1=close 
 * ├─────┤ 
 * │  4  │ 0=IRQ4 open, 1=close
 * ├─────┤
 * │  3  │ 0=IRQ3 open, 1=close
 * ├─────┤
 * │  2  │ 0=IRQ2 open, 1=close
 * ├─────┤
 * │  1  │ 0=IRQ1 open, 1=close
 * ├─────┤
 * │  0  │ 0=IRQ0 open, 1=close
 * └─────┘
 *  OCW1    M    S
 * (ports 0x21,0xA1)
 */

/* ┌─────┐ 
 * │  7  │  R                || |R   SL  EOI  MEANS
 * ├─────┤                   ||A|0   0    1   non-special End-of-Int(full rec)
 * │  6  │  SL               ||A|0   1    1   *special End-of-Int (non-full rec)
 * ├─────┤                   ||B|1   0    1   non-special End-of-Int loop
 * │  5  │  EOI <--- 1=EOI   ||B|1   0    0   auto-end-of-int (seting)   
 * ├─────┤                   ||B|0   0    0   auto-end-of-int (clear)
 * │  4  │  0                ||C|1   1    1   *special end of int loop
 * ├─────┤                   ||C|1   1    0   *set priority command
 * │  3  │  0                || |0   1    0   non-option
 * ├─────┤                   A: end interrupt
 * │  2  │  L2               B: priority auto interrupt
 * ├─────┤                   C: special loop
 * │  1  │  L1
 * ├─────┤
 * │  0  │  L0
 * └─────┘
 *  OCW2    M    S
 * (ports 0x20,0xA0)
 *
 * L0~L2: For priority of IRQ0~IRQ7 (or IRQ8~IRQ15)
 * EOI  : non-auto end of interrupt
 * SL   : priority setting sign
 * R    : priority looping state
 */
//----------------------------------------------------------------------------
// Initialzation Command Word ports
//----------------------------------------------------------------------------
#define PIC0_ICW1 0x20
#define PIC1_ICW1 0xA0

#define PIC0_ICW2 0x21
#define PIC1_ICW2 0xA1

#define PIC0_ICW3 0x21
#define PIC1_ICW3 0xA1

#define PIC0_ICW4 0x21
#define PIC1_ICW4 0xA1
//----------------------------------------------------------------------------
// Operation Control Word ports
//----------------------------------------------------------------------------
#define PIC0_OCW1 0x21
#define PIC1_OCW1 0xA1

#define PIC0_OCW2 0x20
#define PIC1_OCW2 0xA0
//----------------------------------------------------------------------------
#define PIC_MASK_ALL   0xFF  // 1111 1111
#define PIC_OPEN_ALL   0x00  // 0000 0000
#define PIC_OPEN_IRQ0  0xFE  // 1111 1110  clock
#define PIC_OPEN_IRQ1  0xFD  // 1111 1101  keyboard
#define PIC_OPEN_IRQ2  0xFB  // 1111 1011  links slave
#define PIC_OPEN_IRQ4  0xEF  // 1110 1111  serial port1
#define PIC_OPEN_IRQ12 0xEF  // 1110 1111  PS/2 mouse if slave
                            
// -----------------------------------------------------------------
#define PIC_EOI_IRQ1  0x61    // set EOI bit IRQ1 or IRQ8
#define PIC_EOI_IRQ9  0x61    // set EOI bit IRQ1 or IRQ8

#define PIC_EOI_IRQ2   0x62    // set EOI bit IRQ2 or IRQ9
#define PIC_EOI_IRQ10  0x62    // set EOI bit IRQ2 or IRQ9

#define PIC_EOI_IRQ4  0x64    // set EOI bit IRQ4 or IRQ12
#define PIC_EOI_IRQ12 0x64    // set EOI bit IRQ4 or IRQ12
// -----------------------------------------------------------------
// To init 8258A 
void init_8259A();

#endif

