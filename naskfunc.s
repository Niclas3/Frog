; org 0xc400 ;; aka LOADER_BASE_ADDR
; extern HariMain

; global _io_hlt
; global _print
; global _start
; naskfunc

; BootMessage db "hello os" ;len 8
%include "./boot.inc"
section loader vstart=LOADER_BASE_ADDR

LOADER_STACK_TOP  equ  LOADER_BASE_ADDR
jmp loader_start

;; memory descriptor
;; GDT 8 bytes

;; No.0
;; db 1 bytes
;; dw 2 bytes
;; dd 4 bytes
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
; 0000 0000 0000 0000 0000 0000 ; L
; 0000 0000 0000 0000 0000 0000 ; DESC_AVL
; 0000 0000 0000 1111 0000 0000 ; DESC_LIMIT_CODE2
; 0000 0000 0000 0000 1000 0000 ; DESC_P
; 0000 0000 0000 0000 0000 0000 ; DESC_DPL_0
; 0000 0000 0000 0000 0001 0000 ; DESC_S_CODE  ;DT1
; 0000 0000 0000 0000 0000 1000 ; DESC_TYPE_CODE ; TYPE ;X C R A

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

gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

loadermsg db '2 loader in real.'
          db 0

loader_start:
; _start:
; ---------------------------reset black screen---------------------------
; mov al,0x13
; mov ah,0x00
; int 0x10
; mov byte [0x0ff2],8
; mov word [0x0ff4],320
; mov word [0x0ff6],200
; mov dword [0x0ff8],0x000a0000

;----------------------------print message -------------------------------
;     mov ax, loadermsg
;     mov si, ax
; _begin:
;     mov al,[si]
;     add si,1
;     cmp al,0
;     je entry
;
;     mov ah, 0x0e
;     mov bx, 15
;     int 10h
;     jmp _begin
;---------------

    ; mov sp, LOADER_BASE_ADDR
    ; mov bp, loadermsg
    ; mov cx, 17
    ; mov ax, 0x1301
    ; mov bx, 0x001f
    ; mov dx, 0x1800
    ; int 0x10
;-----------------------------------------------------
entry:
in al, 0x92
or al, 0000_0010b
out 0x92, al

;------------------load gdt to gdtr-------------------
lgdt [gdt_ptr]

;------------------cr0 set PE ------------------------
mov eax, cr0
or  eax, 0x00000001
mov cr0, eax

jmp dword SELECTOR_CODE:p_mode_start ; reflash code-flow?

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax

    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    mov byte [gs:160], 'X'
    ; mov byte [gs:280], 'P'
    jmp $
    ; push ebp ; to save old call frame
    ; mov ebp, esp ; initialize new call frame
    ; call HariMain
    ; call _print
;     mov si, ax
; _begin:
;     mov al,[si]
;     add si,1
;     cmp al,0
;
;     je fin
;     mov ah, 0x0e
;     mov bx, 15
;     int 10h
;     jmp _begin
;;--------------------------------------
; fin:
;     hlt
;     jmp fin
;
; _print:
;     mov si, ax
; _begin:
;     mov al,[si]
;     add si,1
;     cmp al,0
;     je fin
;
;     mov ah, 0x0e
;     mov bx, 15
;     int 10h
;     jmp _begin
;----------------------------------------

    ; add esp, 0 ; remove call arguments from frame
    ; pop ebp    ; restore old call frame

    ; mov ebx,0
    ; mov eax,1
    ; int 0x80

; void print

;     mov si, ax
;
;     mov al,[si] 
;     add si,1
;     cmp al,0
;
;     je fin
;     mov ah, 0x0e
;     mov bx, 15
;     int 10h
;     ret

; message :
;      db "z"
;      db 0

; _io_hlt:    ; void io_hlt(void);
;     hlt
;     ret
