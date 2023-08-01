#ifndef KEYBOARD_H
#define KEYBOARD_H
//804x chip
//intel 8042 keyboard controller
//intel 8048 keyboard encoder
#define PORT_KEYDATE         0x0060
//----------------------------------------------
// PORT_KEYSTATE
//----------------------------------------------
// 0x64 port register is a 8-bits
// bit 7 if 1 ,CRC error from keyboard
// bit 6 if 1 ,receive time out(IRQ1 not trigger)
// bit 5 if 1 ,send time out (Keyboard no response)
// bit 4 if 1 ,keyboard locked
// bit 3 if 1 ,data in input buffer is command   (from port 0x64)
//          0 ,data in input buffer is parameter (from port 0x60)
// bit 2 if 1 ,self inspection pass (system flag stats)
//          0 ,start or restart     (system flag stats)
// bit 1 if 1 ,input buffer is full (0x60/64 does give data to 8042)
// bit 0 if 1 ,output buffer is full(0x60 does give data to system)
#define PORT_KEYSTATE        0x0064 //read only
//----------------------------------------------

#define PORT_KEYCOMMD        0x0064 //write only
#define KEYCMD_WRITE_MODE    0x60
#define KBC_MODE             0x47

//-----------------------------------------------
// Keyboard state
//-----------------------------------------------
// TODO: finish all keyboard state
#define KEYSTA_OUTPUT_BUFFER_FULL 0x01
#define KEYSTA_SEND_NOTREADY      0x02

void wait_KBC_sendready();

void init_keyboard();

#endif
