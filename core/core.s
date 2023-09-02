extern UkiMain         ;start symbol of C file

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

extern exception_handler

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
;;----------------------------------------------------------------------------
;; Interrupt handler by Intel init
global  _divide_error
global  _single_step_exception
global  _nmi
global  _breakpoint_exception
global  _overflow
global  _bounds_check
global  _inval_opcode
global  _copr_not_available
global  _double_fault
global  _copr_seg_overrun
global  _inval_tss
global  _segment_not_present
global  _stack_exception
global  _general_protection
global  _page_fault
global  _copr_error
;-----------------------------------------------------------------------------


_start:
    call UkiMain
    jmp $

;-----------------functions-----------------------

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
;; save all context
    push ds
    push es
    push fs
    push gs
    pushad   ;; push 32bits register as order eax,ecx, edx, ebx, esp, ebp, esi, edi
    
    ;;_io_out8(PIC0_OCW2, PIC_EOI_IRQ0); 
    mov al, 0x60
    out 0x20, al

    call inthandler20
    popad
    pop gs
    pop fs
    pop es
    pop ds
    iretd

;;; 0x21 keyboard interrupt handler
_asm_inthandler21:
    ; xchg bx, bx
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

global intr_exit
intr_exit:	     
   add esp, 4			   ; 跳过中断号
   popad
   pop gs
   pop fs
   pop es
   pop ds
   add esp, 4			   ; 跳过error_code
   iretd
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
;-------------------------------------------------------------------------------
;Interrupt handler from orange'os
_divide_error:
	push	0xFFFFFFFF	; no err code
	push	0		; vector_no	= 0
	jmp	_exception
_single_step_exception:
	push	0xFFFFFFFF	; no err code
	push	1		; vector_no	= 1
	jmp	_exception
_nmi:
	push	0xFFFFFFFF	; no err code
	push	2		; vector_no	= 2
	jmp	_exception
_breakpoint_exception:
	push	0xFFFFFFFF	; no err code
	push	3		; vector_no	= 3
	jmp	_exception
_overflow:
	push	0xFFFFFFFF	; no err code
	push	4		; vector_no	= 4
	jmp	_exception
_bounds_check:
	push	0xFFFFFFFF	; no err code
	push	5		; vector_no	= 5
	jmp	_exception
_inval_opcode:
	push	0xFFFFFFFF	; no err code
	push	6		; vector_no	= 6
	jmp	_exception
_copr_not_available:
	push	0xFFFFFFFF	; no err code
	push	7		; vector_no	= 7
	jmp	_exception
_double_fault:
	push	8		; vector_no	= 8
	jmp	_exception
_copr_seg_overrun:
	push	0xFFFFFFFF	; no err code
	push	9		; vector_no	= 9
	jmp	_exception
_inval_tss:
	push	10		; vector_no	= A
	jmp	_exception
_segment_not_present:
	push	11		; vector_no	= B
	jmp	_exception
_stack_exception:
	push	12		; vector_no	= C
	jmp	_exception
_general_protection:
	push	13		; vector_no	= D
	jmp	_exception
_page_fault:
	push	14		; vector_no	= E
	jmp	_exception
_copr_error:
	push	0xFFFFFFFF	; no err code
	push	16		; vector_no	= 10h
	jmp	_exception

_exception:
	call	exception_handler
	add	esp, 4*2
	hlt
