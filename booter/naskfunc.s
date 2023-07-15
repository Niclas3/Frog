%include "../header/boot.inc"
section loader vstart=LOADER_BASE_ADDR ;0xc400
org 0xc400
; LOADER_STACK_TOP  equ  LOADER_BASE_ADDR

jmp loader_start

section .gstack
align 32
GLOBAL_STACK: 
    times 512 db 0
LOADER_STACK_TOP equ $ - GLOBAL_STACK -1

section .r3stack
align 32
RING3_STACK: 
    times 512 db 0
RING3_STACK_TOP equ $ - RING3_STACK-1

;; memory descriptor
;; GDT 8 bytes

;; No.0
;; db 1 bytes
;; dw 2 bytes
;; dd 4 bytes
section .gdt
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
;; No.4            dd  0xa000000F
VGC_DESC         dd  0x0000000F ; limit (0xaffff-0xa0000)/0x1000 = 0xF
                 dd  DESC_VGC_HIGH4

;; No.5
CALL_GATE_DESC dw  (CALL_GATE_TEST & 0xFFFF)
               dw  SELECTOR_RING0_C_C0DE
               dw  CALL_GATE_HIGH2
               dw  ((CALL_GATE_TEST>> 16) & 0xFFFF)

;; No.6 
;;                         base , limit, attr
CODE_DESC_RING3 Descriptor 0    , code_in_ring3_len -1   , 0x4000+0x98+60h
;; No.7 
STACK_DESC_RING3     Descriptor 0    , RING3_STACK_TOP, 0x93+0x4000+0x60

;; No.8
TSS_DESC           Descriptor 0    , TSS_Len-1, 0x89

;; No.9
C_RING0_CODE_DESC  dd  0x0000FFFF
                   dd  DESC_C_CODE_HIGH4


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

SELECTOR_CODE        equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA        equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO       equ (0x0003<<3) + TI_GDT + RPL0
SELECTOR_VGC         equ (0x0004<<3) + TI_GDT + RPL0
SELECTOR_CALL_GATE   equ (0x0005<<3) + TI_GDT + RPL0
SELECTOR_CODE_RING3  equ (0x0006<<3) + TI_GDT + RPL3
SELECTOR_STACK_RING3 equ (0x0007<<3) + TI_GDT + RPL3
SELECTOR_TSS         equ (0x0008<<3) + TI_GDT + RPL0
SELECTOR_RING0_C_C0DE equ (0x0009<<3) + TI_GDT + RPL0

gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

;;TSS
section .tss
LABEL_TSS:
         dd 0                   ;; Previous Task link
         dd LOADER_STACK_TOP    ;; ESP0
         dd SELECTOR_DATA       ;; SS0
         dd 0                   ;; ESP1
         dd 0                   ;; SS1
         dd 0                   ;; ESP2
         dd 0                   ;; SS2
         dd 0                   ;; CR3(PDBR)
         dd 0                   ;; EIP
         dd 0                   ;; EFLAGS
         dd 0                   ;; EAX
         dd 0                   ;; ECX 
         dd 0                   ;; EDX
         dd 0                   ;; EBX 
         dd 0                   ;; ESP
         dd 0                   ;; EBP
         dd 0                   ;; ESI
         dd 0                   ;; EDI
         dd 0                   ;; ES
         dd 0                   ;; CS
         dd 0                   ;; SS
         dd 0                   ;; DS
         dd 0                   ;; FS
         dd 0                   ;; GS
         dd 0                   ;; LDT Segment Selector
         dw 0                   ;; T
         dw $-LABEL_TSS +2      ;; I/O map base address 
         db 0ffh
         ; dd 0                   ;; SSP
TSS_Len equ $-LABEL_TSS

;;END TSS

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
    nop
    ; mov al,0x13
    ; mov ah,0x00
    ; int 0x10
;----------------------------------------------------

; Init 32 bits code description 0x01
    xor eax, eax
    mov ax, cs
    shl eax, 4
    ; add eax, LABEL_SEG_CODE32  ;; start of code segments
    add eax, 0x0                 ;; start of code segments base address
    mov word [CODE_DESC+ 2], ax
    shr eax, 16
    mov byte [CODE_DESC+ 4], al
    mov byte [CODE_DESC+ 7], ah

; Init 32 bits stack description 0x02
    xor eax, eax
    mov ax, cs
    shl eax, 4
    add eax, GLOBAL_STACK                 ;; start of code segments base address
    mov word [DATA_STACK_DESC+ 2], ax
    shr eax, 16
    mov byte [DATA_STACK_DESC+ 4], al
    mov byte [DATA_STACK_DESC+ 7], ah

; Init 32 bits call gate code description 0x05
    xor eax, eax
    mov eax, _CALL_GATE_TEST; offset
    mov word [CALL_GATE_DESC], ax
    shr eax, 16
    mov word [CALL_GATE_DESC+6], ax

; Init 32 bits ring3 code description 0x06
    xor eax, eax
    mov ax, cs
    shl eax, 4
    add eax, _code_in_ring3     ;; start of code segments base address
    mov word [CODE_DESC_RING3 + 2], ax
    shr eax, 16
    mov byte [CODE_DESC_RING3 + 4], al
    mov byte [CODE_DESC_RING3 + 7], ah

; Init 32 bits stack description 0x07
    xor eax, eax
    mov ax, cs
    shl eax, 4
    add eax, RING3_STACK; start of code segments base address
    mov word [STACK_DESC_RING3+ 2], ax
    shr eax, 16
    mov byte [STACK_DESC_RING3+ 4], al
    mov byte [STACK_DESC_RING3+ 7], ah

; Init 32 bits stack description 0x08
    xor eax, eax
    mov ax, cs
    shl eax, 4
    add eax, LABEL_TSS ;; start of code segments base address
    mov word [TSS_DESC + 2], ax
    shr eax, 16
    mov byte [TSS_DESC + 4], al
    mov byte [TSS_DESC + 7], ah

; Init IDT at 0x00
    xor eax, eax
    mov eax, _Putchar  ; offset
    mov word [IDT_BASE], ax
    shr eax, 16
    mov word [IDT_BASE+6], ax

; Init IDT at 0x01
    xor eax, eax
    mov eax, _ClockHandler; offset
    mov word [IDT_BASE+8], ax
    shr eax, 16
    mov word [IDT_BASE+6+8], ax

; Init IDT at 0x20  time interrupt IRQ0
    xor eax, eax
    mov eax, _ClockHandler; offset
    mov word [IDT_BASE+(8*32)], ax
    shr eax, 16
    mov word [IDT_BASE+6+(8*32)], ax

; Init IDT at 0x21  keyboard interrupt IRQ1
    xor eax, eax
    mov eax, _KeyboardHandler; offset
    mov word [IDT_BASE+(8*0x21)], ax
    shr eax, 16
    mov word [IDT_BASE+6+(8*0x21)], ax

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

;------------------init idt---------------------------
init_idt:
cli
lidt [idt_ptr]

;-----------------------------------------------------
jmp dword SELECTOR_CODE: LABEL_SEG_CODE32; reflash code-flow?

;;TODO how to set specific section 
; jmp dword SELECTOR_CODE: 0; reflash code-flow? 

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
    call Init8259A
    ; int 00h
    ; int 01h
    sti
    mov ax, SELECTOR_TSS
    ltr ax

    push SELECTOR_STACK_RING3
    push RING3_STACK_TOP
    push SELECTOR_CODE_RING3
    push 0
    retf
    ; call SELECTOR_CALL_GATE:0xe000
    jmp $
;;;;;; Jump to core.s this is real os code
;  0xe000 = 0x6000                        - 0x200               + 0x8200
;          (address in a.img of elf .text)  (the top 512 is IPL)  (load code to this )
    ; jmp dword SELECTOR_CODE:(0xe000-$$)
    jmp dword SELECTOR_CODE: 0xe000

_Putchar:
Putchar equ _Putchar - $$
    mov ah, 0ch
    mov al, `!`
    mov word [gs:160], ax
    ; jmp $
    iretd

_ClockHandler:
ClockHandler equ _ClockHandler - $$
    ; mov byte [eax+edx*320+0], 8
    ; there is no end!!
    ; cmp edx, 168
    ; jnle _clock_test_end
    ; mov eax, [VRAM]
    ; imul ecx, edx, 320
    ; add ecx, edx
    ; add eax, ecx
    ; mov byte [eax], 8
    ; inc edx
    ; _clock_test_end:
    ; mov al,20h
    ; out 20h, al
    inc word [gs:160]
    mov al,20h
    out 20h, al
    iretd

_KeyboardHandler:
KeyboardHandler equ _KeyboardHandler- $$
    ; mov eax, [VRAM]
    ; imul ecx, edx, 400
    ; add ecx, edx
    ; add eax, ecx
    ; mov byte [eax], 10
    ; inc edx
    inc word [gs:260]

    cli            ; Disable interrupts
empty_buffer:
    in al, 0x64    ; Read keyboard status register into AL
    test al, 0x01  ; Check bit 0 of AL (keyboard status)
    jz buffer_empty ; If zero, jump to buffer_empty (buffer is empty)

    in al, 0x60    ; Read keyboard data register into AL
    jmp empty_buffer ; Repeat until buffer is empty

buffer_empty:
    sti            ; Enable interrupts
    mov al,20h
    out 20h, al
    iretd


_CALL_GATE_TEST:
CALL_GATE_TEST equ _CALL_GATE_TEST- $$
    mov ah, 0ch
    mov al, `C`
    mov word [gs:300], ax
    jmp $
    retf

_code_in_ring3:
    mov ah, 0ch
    mov al, `3`
    mov word [gs:310], ax
    jmp SELECTOR_CALL_GATE:0xe000
    ; call SELECTOR_CALL_GATE:0xe000

    jmp $
code_in_ring3_len equ $-_code_in_ring3

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

    mov al, 1111_1101b
    out 021h, al
    call io_delay

    mov al, 1111_1111b
    out 0A1h, al
    call io_delay
    ret

io_delay:
    nop
    nop
    nop
    nop
    ret

