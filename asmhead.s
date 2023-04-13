CYLS  equ  0x0ff0
LEDS  equ  0x0ff1
VMODE equ  0x0ff2
SCRNX equ  0x0ff4
SCRNY equ  0x0ff6
VRAM  equ  0x0ff8

org 0xca00   ;core at ca00
mov al,0x13
mov ah,0x00
int 0x10
mov byte [VMODE],8
mov word [SCRNX],320
mov word [SCRNY],200
mov dword [VRAM],0x000a0000

mov ah,0x02
int 0x16
mov [LEDS],al
fin:
    hlt
    jmp fin
