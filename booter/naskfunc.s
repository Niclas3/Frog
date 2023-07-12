%include "../header/boot.inc"
section loader vstart=LOADER_BASE_ADDR ;0xc400

LOADER_STACK_TOP  equ  LOADER_BASE_ADDR
org 0xc400
jmp loader_start

;; memory descriptor
;; GDT 8 bytes

;; No.0
;; db 1 bytes
;; dw 2 bytes
;; dd 4 bytes
; [section .gdt]
GDT_BASE   dd  0x00000000  ; low
           dd  0x00000000  ; high

;; No.1 
CODE_DESC  dd  0x0000FFFF
           dd  DESC_CODE_HIGH4


; 24 bits high
;        |
; 0000 0000 0000 0000 0000 0000
; ;----new----------------------------------------
; 0000 0000 1000 0000 0000 0000 ; DESC_G_4K
; 0000 0000 0100 0000 0000 0000 ; DESC_D_32
; 0000 0000 0000 0000 1000 0000 ; DESC_P
; 0000 0000 0000 0000 0000 0000 ; DESC_AVL
; 0000 0000 0000 1111 0000 0000 ; DESC_LIMIT_CODE2
; 0000 0000 0000 0000 0000 0000 ; DESC_DPL_0
; 0000 0000 0000 0000 0001 0000 ; DESC_S_CODE  ;DT1
; 0000 0000 0000 0000 0000 1000 ; DESC_TYPE_CODE ; TYPE ;X C R A
; 0000 0000 0000 0000 0000 0000 ; L

; 0000 0000 1100 1111 1001 1000
;           C    F    9    8

; ;----old-----------------------------------------
; 0000 0000 0000 0000 0000 0000
; 1000 0000 0000 0000 0000 0000 ; DESC_G_4K
; 0100 0000 0000 0000 0000 0000 ; DESC_D_32
; 0000 0000 1000 0000 0000 0000 ; DESC_P
; 0000 0000 0000 0000 0000 0000 ; DESC_AVL
; 0000 1111 0000 0000 0000 0000 ; DESC_LIMIT_CODE2
; 0000 0000 0000 0000 0000 0000 ; DESC_DPL_0
; 0000 0000 0001 0000 0000 0000 ; DESC_S_CODE
; 0000 0000 0000 1000 0000 0000 ; DESC_TYPE_CODE

;;0x00

;; No.2 
DATA_STACK_DESC  dd  0x0000FFFF
                 dd  DESC_DATA_HIGH4

;; No.3  8 bytes
; VIDEO_DESC       dd  0xb8000007
VIDEO_DESC       dd  0x80000007
                 dd  DESC_VIDEO_HIGH4
VGC_DESC         dd  0x0000000F ; limit (0xaffff-0xa0000)/0x1000 = 0xF
                 dd  DESC_VGC_HIGH4
;;END GDT
;;---------------------------------

GDT_SIZE equ $ - GDT_BASE
;; for gdtr 32-bit + 16-bit
GDT_LIMIT equ GDT_SIZE - 1

times 60 dq 0
;;; 15-3                             2        1-0
;;; descriptor index                TI        RPL
;;; TI == 0 in GDT
;;; TI == 1 in LDT

SELECTOR_CODE  equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA  equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0
SELECTOR_VGC   equ (0x0004<<3) + TI_GDT + RPL0

gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

;; IDT
section .idt
; [bits 32]
IDT_BASE:
%rep 255
         dw  (Putchar & 0xFFFF)
         dw  SELECTOR_CODE
         dw  INTR_GATE_HIGH2
         dw  ((Putchar >> 16) & 0xFFFF)
%endrep

IDT_SIZE equ $$-IDT_BASE
IDT_LIMIT equ IDT_SIZE - 1

idt_ptr  dw IDT_LIMIT
         dd IDT_BASE

;; END IDT

; [section .s16]
; [bits 16]
loadermsg db '2 loader in real.'
          db 0

loader_start:
;----------------------------print message -------------------------------
mov byte [VMODE],8
mov word [SCRNX],320
mov word [SCRNY],200
mov dword [VRAM],0x000a0000
;----------------------------print message -------------------------------
print_begin:
    mov ax, loadermsg
    mov si, ax
_print_begin:
    mov al,[si]
    add si,1
    cmp al,0
    je rst_b_scr

    mov ah, 0x0e
    mov bx, 15
    int 10h
    jmp _print_begin

;--reset black screen---------------------------
rst_b_scr:
    ; nop
    mov al,0x13
    mov ah,0x00
    int 0x10
;----------------------------------------------------

; Init 32 bits code description
    xor eax, eax
    mov ax, cs
    shl eax, 4
    add eax, LABEL_SEG_CODE32
    mov word [CODE_DESC+ 2], ax
    shr eax, 16
    mov byte [CODE_DESC+ 4], al
    mov byte [CODE_DESC+ 7], ah
; A20 open

in al, 0x92
or al, 0000_0010b
out 0x92, al

;------------------load gdt to gdtr-------------------
lgdt [gdt_ptr]

;------------------cr0 set PE ------------------------
mov eax, cr0
or  eax, 0x00000001
mov cr0, eax

;--------------- init idt ----------------------
init_idt:
cli
lidt [idt_ptr]

;-----------------------------------------------------
; jmp dword SELECTOR_CODE: LABEL_SEG_CODE32; reflash code-flow?
jmp dword SELECTOR_CODE: 0; reflash code-flow?

section .s32
bits 32
LABEL_SEG_CODE32:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov byte [gs:160], 'Z'
    ; call Init8259A
    int 80h
;;;;;; Jump to core.s this is real os code
;  0xe000 = 0x6000                        - 0x200               + 0x8200
;          (address in a.img of elf .text)  (the top 512 is IPL)  (load code to this )
    jmp dword SELECTOR_CODE:(0xe000-$$)

_Putchar:
Putchar equ _Putchar - $$
    mov ah, 0ch
    mov al, `!`
    mov word [gs:160], ax
    ; jmp $
    iretd

Init8259A:
    mov al, 011h
    out 020h, al
    call io_delay

    out 0A0h, al
    call io_delay

    mov al, 020h
    out 021h, al
    call io_delay

    mov al, 028h
    out 0A1h, al
    call io_delay

    mov al, 004h
    out 021h, al
    call io_delay

    mov al, 002h
    out 0A1h, al
    call io_delay

    mov al, 001h
    out 021h, al
    call io_delay

    out 0A1h, al
    call io_delay

    mov al, 11111110b
    out 021h, al
    call io_delay


    mov al, 11111111b
    out 0A1h, al
    call io_delay
    ret

io_delay:
    nop
    nop
    nop
    nop
    ret

