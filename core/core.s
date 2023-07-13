%include "../header/boot.inc"
extern HariMain

global _start

global _write_mem8      ;_void _write_mem8(int addr, int data);
global _io_hlt
global _io_cli,   _io_sti,  _io_stihlt
global _io_in8,  _io_in16,  _io_in32
global _io_out8, _io_out16, _io_out32
global _io_load_eflags, _io_store_eflags

SELECTOR_VGC   equ (0x0004<<3) + TI_GDT + RPL0
_start:
    call HariMain
    jmp $

;-----------------functions-----------------------
_write_mem8:
    mov ecx,[esp+4]
    mov al,[esp+8]
    mov dx, SELECTOR_VGC
    mov edi,ecx
    mov gs, dx
    mov byte [gs:ecx], al
    ret

_io_hlt:    ; void io_hlt(void);
    hlt
    ret

_io_cli:    ; void io_cli(void)
    cli     ; clear interrupt eflags
    ret

_io_sti:    ; void io_sti(void)
    sti     ; set interrupt eflags
    ret

_io_stihlt: ; void io_stihlt(void)
    sti
    hlt
    ret

_io_in8:    ; void io_in8(int port)
    mov edx,[esp+4]   ;port
    mov eax,0
    in  al,dx
    ret

_io_in16:    ; void io_in16(int port)
    mov edx,[esp+4]   ;port
    mov eax,0
    in  al,dx
    ret

_io_in32:    ; void io_in32(int port)
    mov edx,[esp+4]   ;port
    in eax,dx
    ret

_io_out8:    ; void io_out8(int port, int data)
    mov edx,[esp+4]  ; port
    mov al,[esp+8]   ; data
    out dx, al
    ret

_io_out16:    ; void io_out16(int port, int data)
    mov edx,[esp+4]  ; port
    mov al,[esp+8]   ; data
    out dx, ax
    ret

_io_out32:    ; void io_out32(int port, int data)
    mov edx,[esp+4]  ; port
    mov eax,[esp+8]   ; data
    out dx, eax
    ret

_io_load_eflags: ; int io_load_eflags(void)
    pushfd       ; push flags double-word
    pop eax
    ret

_io_store_eflags: ; void io_store_eflags(int eflags)
    mov eax, [esp+4]
    push eax
    popfd      ; pop flags double-word
    ret