#include "../include/int.h"
#include "../include/graphic.h"

/* 1.There are 3 function cooperating each other. */
/*   _asm_inthandler21: core.s */
/*   inthandler21     : interrupt/int.c */
/*   create_gate      : destab/descriptor.c */
/*  */
/*     `_asm_inthandler**()` is the real callback function for interrupt, */
/*   according to interrupt callback function need to write in assemble */
/*   code, `_asm_inthandler**()` contains a C code to do some high level */
/*   things. This contained function is `inthandler**()`, you can add */
/*   function at high level C code for interrupt. */
/*     If you want to add a new interrupt handle code.First, you should add */
/*   assemble function at core.s and name it `_asm_inthandler**()`the `**` */
/*   represents hex number of this interrupt.  */
/*     Then, you should call `create_gate()` to register this assemble  */
/*   handler. Third, don't forget add a C code function at `interrupt/int.c` */
/*   as `inthandler**()`, and put it into `core.s:_asm_inthandler**()` */
/*     Finally, you finish the interrupt setting. */

void inthandler21(){
    putfonts8_asc((char *)0xa0000, 320, 8, 8, COL8_0000FF, "Niclas 123");
}
