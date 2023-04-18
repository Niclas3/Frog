; naskfunc
extern HariMain

global _io_hlt
global _start

[section text]

_start:
    call HariMain
    add esp, 8

    mov ebx,0
    mov eax,1
    int 0x80

_io_hlt:    ; void io_hlt(void);
    hlt
    ret
