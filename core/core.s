extern UkiMain         ;start symbol of C file

;;----------------------------------------------------------------------------
;; Interrupt entry handler by Intel init
  global intr_entry_table
;---------------------------------------------------------------
; All interrupt real handlers table
; register C function into this global table*/
  extern intr_table
;---------------------------------------------------------------

;---------------------------------------------------------------
extern syscall_table
global syscall_handler

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
global intr_exit
;-----------------------------------------------------------------------------
INT_VEC_SYS_CALL equ 0x93
;-----------------------------------------------------------------------------
section .data
intr_entry_table:

%define PUSH_ZERO_ERRCODE push 0
%define PUSH_FULL1_ERRCODE push 0xFFFFFFFF
%define DO_NOTHING nop

;;filler function intr_entry_table
%macro INTR_FILLER_DWORD 1
section .data
    resd %1
%endmacro

;Create interrupt handler at master pic
; @param %1 interrupt number
; @param %2 error code
; @param %3 EOI(end of interrupt)
;
%macro INTR_M_HANDLER 3
;; save all context
section .text
_asm_inthandler%1:
    %2     ;;push error code
    push ds
    push es
    push fs
    push gs
    pushad   ;; push 32bits register as order eax,ecx, edx, ebx, esp, ebp, esi, edi
    push %1; ;; push interrupt number

    mov al, %3
    out 0x20, al  ;; send ack to master

    call [intr_table+%1*4]
    jmp intr_exit
section .data
    dd _asm_inthandler%1 ;; each interrupt entry 
%endmacro

;Create interrupt handler at slave pic
; @param %1 interrupt number
; @param %2 error code
; @param %3 master EOI(end of interrupt)
; @param %4 slave  EOI(end of interrupt)
;
%macro INTR_S_HANDLER 4
;; save all context
section .text
_asm_inthandler%1:
    %2     ;; error code
    push ds
    push es
    push fs
    push gs
    pushad   ;; push 32bits register as order eax,ecx, edx, ebx, esp, ebp, esi, edi
    push %1; ;; push interrupt number

    mov al, %4
    out 0xa0, al  ;; send ack to slaver
    mov al, %3
    out 0x20, al  ;; send ack to master

    call [intr_table+%1*4]
    jmp intr_exit
section .data
    dd _asm_inthandler%1 ;; each interrupt entry 
%endmacro


%macro EXCEPTION_HANDLER 2
section .text
_asm_exceptionhandler%1:
        xchg bx, bx
	%2              ; err code
	push	%1              ; vector_no 
	call	exception_handler
	add	esp, 4*2
	hlt
section .data
    dd _asm_exceptionhandler%1 ;; each exception entry 
%endmacro


section .text
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
;; 0x93 handler system call
syscall_handler:
;1. save context
    push 0 ;; push error code
    push ds
    push es
    push fs
    push gs
    pushad   ;; push 32bits register as order eax,ecx, edx, ebx, esp, ebp, esi, edi
    push INT_VEC_SYS_CALL ;; number of interrupt
;2. pass arguments
;I only support 4 arguments
; first at ebx, second at ecx, third at edx
    push esi   ; 4th
    push edx   ; 3th
    push ecx   ; 2nd
    push ebx   ; 1st 
;3. call sub-routine according to eax which is subroutine number at
; syscall_table
; eax is syscall number,
; syscall_table contains function pointer which size is 4 bytes
    call [syscall_table + eax * 4]
    add esp, 4*4
;4. return value at eax,
;   esp+8*4 is eax 
; pushad push 8 registers
    mov [esp+8*4], eax
    jmp intr_exit

;-------------------------------------------------------------------------------
;Interrupt handler from orange'os
;-------------------------------------------------------------------------------
; _divide_error:
; 	push	0xFFFFFFFF	; no err code
; 	push	0		; vector_no	= 0
; 	jmp	_exception
;0x0 divide error 
EXCEPTION_HANDLER 0x0, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _single_step_exception:
; 	push	0xFFFFFFFF	; no err code
; 	push	1		; vector_no	= 1
; 	jmp	_exception
; section .data
;     dd 0xffffffff ;; each exception entry 
;0x1 single_step_exception
EXCEPTION_HANDLER 0x1, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _nmi:
; 	push	0xFFFFFFFF	; no err code
; 	push	2		; vector_no	= 2
; 	jmp	_exception
;0x2 Non-maskable interrupt
EXCEPTION_HANDLER 0x2, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _breakpoint_exception:
; 	push	0xFFFFFFFF	; no err code
; 	push	3		; vector_no	= 3
; 	jmp	_exception
; 0x3 break point exception
EXCEPTION_HANDLER 0x3, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _overflow:
; 	push	0xFFFFFFFF	; no err code
; 	push	4		; vector_no	= 4
; 	jmp	_exception
; 0x4 over flow exception
EXCEPTION_HANDLER 0x4, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _bounds_check:
; 	push	0xFFFFFFFF	; no err code
; 	push	5		; vector_no	= 5
; 	jmp	_exception
; 0x5 bounds check
EXCEPTION_HANDLER 0x5, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _inval_opcode:
; 	push	0xFFFFFFFF	; no err code
; 	push	6		; vector_no	= 6
; 	jmp	_exception
; 0x6 inval opcode
EXCEPTION_HANDLER 0x6, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _copr_not_available:
; 	push	0xFFFFFFFF	; no err code
; 	push	7		; vector_no	= 7
; 	jmp	_exception
; 0x7 copr not available 
EXCEPTION_HANDLER 0x7, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _double_fault:
; 	push	8		; vector_no	= 8
; 	jmp	_exception
; 0x8 double fault
EXCEPTION_HANDLER 0x8, DO_NOTHING
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _copr_seg_overrun:
; 	push	0xFFFFFFFF	; no err code
; 	push	9		; vector_no	= 9
; 	jmp	_exception
; 0x9 copr segment overrun
EXCEPTION_HANDLER 0x9, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _inval_tss:
; 	push	10		; vector_no	= A
; 	jmp	_exception
; 0xa inval TSS
EXCEPTION_HANDLER 0xa, DO_NOTHING
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _segment_not_present:
; 	push	11		; vector_no	= B
; 	jmp	_exception
; 0xb segment not present
EXCEPTION_HANDLER 0xb, DO_NOTHING
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _stack_exception:
; 	push	12		; vector_no	= C
; 	jmp	_exception
; 0xc stack exception
EXCEPTION_HANDLER 0xc, DO_NOTHING
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _general_protection:
; 	push	13		; vector_no	= D
; 	jmp	_exception
; 0xd general protection
EXCEPTION_HANDLER 0xd, DO_NOTHING
;-------------------------------------------------------------------------------

;-------------------------------------------------------------------------------
; _page_fault:
; 	push	14		; vector_no	= E
; 	jmp	_exception
; 0xd page fault
EXCEPTION_HANDLER 0xe, DO_NOTHING
;-------------------------------------------------------------------------------
;; fill one dword for intr_entry_table
INTR_FILLER_DWORD 1

; _copr_error:
; 	push	0xFFFFFFFF	; no err code
; 	push	16		; vector_no	= 10h
; 	jmp	_exception
; 0x10 copr error
EXCEPTION_HANDLER 0x10, PUSH_FULL1_ERRCODE
;-------------------------------------------------------------------------------

;;filler function for intr_entry_table
INTR_FILLER_DWORD 0xf

;;; 0x20 Clock interrupt handler
INTR_M_HANDLER 0x20, PUSH_ZERO_ERRCODE, 0x60

;;; 0x21 keyboard interrupt handler
INTR_M_HANDLER 0x21, PUSH_ZERO_ERRCODE, 0x61

;;filler function for intr_entry_table
INTR_FILLER_DWORD 10

;;; 0x2C PS/2 Mouse handler
INTR_S_HANDLER 0x2c, PUSH_ZERO_ERRCODE,  0x62, 0x64

;;filler function intr_entry_table
INTR_FILLER_DWORD 1

;; 0x2e primary channel handler
INTR_S_HANDLER 0x2e, PUSH_ZERO_ERRCODE, 0x62, 0x66

;; 0x2f secondary channel handler
INTR_S_HANDLER 0x2f, PUSH_ZERO_ERRCODE, 0x62, 0x67

;;------------------------------------------------------------------------------
intr_exit:
   add esp, 4			   ; 跳过中断号
;; push 32bits register as order 
;; eax,ecx, edx, ebx, esp, ebp, esi, edi
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
