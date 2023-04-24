%include "./boot.inc"
extern HariMain
global _start
global _write_mem8 ;_void _write_mem8(int addr, int data);
global _io_hlt
; [section start] vstart=0xd400

SELECTOR_VGC   equ (0x0004<<3) + TI_GDT + RPL0
_start:
    call HariMain
    jmp $

_write_mem8:
    mov ecx,[esp+4]
    mov al,[esp+8]
    mov dx, SELECTOR_VGC
    mov edi,ecx
    mov gs, dx
    ; mov byte [gs:0], al
    mov byte [gs:ecx], al
    ; mov [ecx],al
    ret

_io_hlt:    ; void io_hlt(void);
    hlt
    ret
