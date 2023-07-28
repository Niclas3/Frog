%include "../header/boot.inc"
extern HariMain

;---------------------------------------------------------------
; Export Interrupt C code handler
;---------------------------------------------------------------
;      Keyboard     ;      Clock  ;      PS/2 Mouse     
extern inthandler21, inthandler20, inthandler2C
;---------------------------------------------------------------
;---------------------------------------------------------------
; Globale Interrupt real handler 
;---------------------------------------------------------------
;      Keyboard         ;      Clock        ;      PS/2 Mouse    
global _asm_inthandler21, _asm_inthandler20, _asm_inthandler2C
;---------------------------------------------------------------

global _start

global _write_mem8      ;_void _write_mem8(int addr, int data);
global _io_hlt
global _io_cli,   _io_sti,  _io_stihlt
global _io_in8,  _io_in16,  _io_in32
global _io_out8, _io_out16, _io_out32
global _io_load_eflags, _io_store_eflags
global _io_delay

global _load_idtr, _save_idtr
global _load_gdtr, _save_gdtr


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
;;------------------------------------------------------------------------------
;;I/O operator IN 
;;------------------------------------------------------------------------------
_io_in8:    ; void io_in8(int port)
    mov edx,[esp+4]   ;port
    mov eax,0   ;data
    in  al,dx
    ret

_io_in16:    ; void io_in16(int port)
    mov edx,[esp+4]   ;port
    mov eax,0         ;data
    in  al,dx
    ret

_io_in32:    ; void io_in32(int port)
    mov edx,[esp+4]   ;port
    mov edx,0        ;data
    in eax,dx
    ret
;;------------------------------------------------------------------------------

;;------------------------------------------------------------------------------
;;I/O operator  OUT
;;------------------------------------------------------------------------------
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
;-------------------------------------------------------------------------------

_io_load_eflags: ; int io_load_eflags(void)
    pushfd       ; push flags double-word
    pop eax
    ret

_io_store_eflags: ; void io_store_eflags(int eflags)
    mov eax, [esp+4]
    push eax
    popfd      ; pop flags double-word
    ret

_io_delay:
    nop
    nop
    nop
    nop
    ret
;;------------------------------------------------------------------------------

;;------------------------------------------------------------------------------
;;
;;                             Interrupt handler
;;
;;------------------------------------------------------------------------------
;;------------------------------------------------------------------------------

;;; 0x20 Clock interrupt handler
_asm_inthandler20:
    ; PUSH	ES
    ; PUSH	DS
    ; PUSHAD
    ; MOV		EAX,ESP
    ; PUSH	EAX
    ; MOV		AX,SS
    ; MOV		DS,AX
    ; MOV		ES,AX
    CALL	inthandler20
    ; POP		EAX
    ; POPAD
    ; POP		DS
    ; POP		ES
    mov al,20h
    out 20h, al
    IRETD

;;; 0x21 keyboard interrupt handler
_asm_inthandler21:
    call inthandler21
    IRETD

;;; 0x2C PS/2 Mouse handler
_asm_inthandler2C:
    ; PUSH	ES
    ; PUSH	DS
    ; PUSHAD
    ; MOV		EAX,ESP
    ; PUSH	EAX
    ; MOV		AX,SS
    ; MOV		DS,AX
    ; MOV		ES,AX
    CALL inthandler2C
    ; POP		EAX
    ; POPAD
    ; POP		DS
    ; POP		ES
    mov al,20h
    out 20h, al
    IRETD
;;------------------------------------------------------------------------------

;;Function for GDT and IDT
;;load data to register
;;save data from register
_load_gdtr:  ;void load_gdtr(int_16 limit, int addr);
    mov ax, [esp+4] ; limit
    mov [esp+6],ax
    lgdt [esp+6]
    ret

_save_gdtr:  ;void save_gdtr(int_32* address);
    mov eax, [esp+4]
    sgdt [eax] ; address
    ret

_load_idtr:  ;void load_idtr(int limit, int addr);
    mov ax, [esp+4] ; limit
    mov [esp+6],ax
    cli             ; it should be clear interrupter
    lidt [esp+6]
    ret

_save_idtr:  ;void save_idtr(int_32* address);
    mov eax, [esp+4]
    sidt [eax] ; address
    ret
