org 0x7c00
; db  0xeb, 0x4e, 0x90
; db "HELLOIPL"
; dw 512
; db 1
; dw 1
; db 2
; dw 224
; dw 2880
; db 0xf0
; dw 9
; dw 18
; dw 2
; dd 0
; dd 2880
; db 0,0,0x29
; dd 0xffffffff
; 		DB		"HELLO-OS   "	; 僨傿僗僋偺柤慜乮11僶僀僩乯
; db "FAT12abc"
; resb 18

jmp msg
; db 0x90

entry:
    mov ax,0
    mov ss,ax
    mov sp,0x7c00
    mov ds,ax
    mov es,ax
    mov si,msg

mov ax,0x0820
mov es,ax
mov ch,0
mov dh,0
mov cl,2

mov ah,0x02
mov al,1
mov bx,0
mov dl,0x00
int 0x13
jc error

putloop:
    mov al,[si]
    add si,1
    cmp al,0
    je fin
    mov ah,0x0e
    mov bx,0
    int 0x10
    jmp putloop

fin:
    hlt
    jmp fin

error:
    mov si,msg

msg:
    DB 0x0a, 0x0a
    DB "load error"
    DB 0x0a
    db 0

resb 0x1fe-($-$$)
; times 510-($-$$) db 0
db 0x55, 0xaa
; dw 0xaa55
; db 0xf0, 0xff, 0xff, 0x00, 0x00,0x00,0x00,0x00
; resb 4600
; db 0xf0, 0xff, 0xff, 0x00, 0x00,0x00,0x00,0x00
; resb 1469432

