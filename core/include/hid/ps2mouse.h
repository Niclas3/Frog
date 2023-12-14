#ifndef PS2MOUSE_H
#define PS2MOUSE_H

#define MOUSE_WRITE      0xd4
#define MOUSE_ENABLE     0xf4

#define KEYCMD_SENDTO_MOUSE 0xd4
#define MOUSECMD_ENABLE     0xf4

void enable_mouse(void);

/* int 0x2C;
 * Interrupt handler for PS/2 mouse
 **/
void inthandler2C(void);

#endif
