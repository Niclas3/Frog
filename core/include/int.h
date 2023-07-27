#ifndef INT_H
#define INT_H

/* int 0x20;
 * Interrupt handler for inner Clock
 **/
void inthandler20();
// void niclas_clock_handler();
/*
 * int 0x21; 
 * Interrupt handler for Keyboard
 **/
void inthandler21();

/* int 0x2C;
 * Interrupt handler for PS/2 mouse
 **/
// void inthandler2C();
#endif
