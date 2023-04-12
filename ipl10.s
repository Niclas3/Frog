org 0x7c00
CYLS equ 10
db  0xeb, 0x4e, 0x90
db "HELLOIPL"
dw 512
db 1
dw 1
db 2
dw 224
dw 2880
db 0xf0
dw 9
dw 18
dw 2
dd 0
dd 2880
db 0,0,0x29
dd 0xffffffff
		DB		"HELLO-OS   "	; 僨傿僗僋偺柤慜乮11僶僀僩乯
db "FAT12abc"
resb 18

entry:
    mov ax,0
    mov ss,ax
    mov sp,0x7c00
    mov ds,ax
    mov es,ax

;read c0-h0-s2 sector
mov ax,0x0820
mov es,ax
mov ch,0  ; cylinder
mov dh,0  ; head
mov cl,2  ; sector

start:
    mov si,0  ;retry counter

retry:
    mov ah,0x02 ; read sector
    mov al,1    ; read 1 sector
    mov bx,0    ; director memory address
    mov dl,0x00 ; select the first dirver
    int 0x13
    jnc next
    add si,1
    cmp si,5
    jae error
    mov ah,0x00
    mov dl,0x00
    int 0x13    ; reset dirver
    jmp retry
next:
    mov ax,es
    add ax,0x0020 ; add 512 Byte size
    mov es,ax
    add cl,1
    cmp cl,18
    jbe start

    mov cl,1
    add dh,1
    cmp dh,2
    jb start
    mov dh,0
    add ch,1
    cmp ch,CYLS
    jb start
    
    jmp 0x8200 ;jump to os starter?

fin:
    hlt
    jmp fin

error:
    mov si,msg
putloop:
    mov al,[si]
    add si,1
    cmp al,0
    je fin
    mov ah,0x0e
    mov bx,0
    int 0x10
    jmp putloop
msg:
    DB 0x0a, 0x0a
    DB "load error"
    DB 0x0a
    db 0

resb 0x1fe-($-$$)
; times 510-($-$$) db 0
db 0x55, 0xaa
