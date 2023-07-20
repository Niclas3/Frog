org 0x7c00
%include "../header/boot.inc"
mov eax, 2  ;; LOADER_START_SECTOR
mov bx, LOADER_BASE_ADDR
mov cx, 10
call read_hard_disk_16
jmp LOADER_BASE_ADDR

;------------------------------------------------------------
; Read n sector from hard disk 
read_hard_disk_16:
;------------------------------------------------------------
;; eax = LBA sector number
;; bx  = base address 
;; cx  = read-in sector number
    mov esi, eax
    mov di, cx
;;Read disk
;;No1: Set sector count for reading
    mov dx, 0x1f2
    mov al, cl
    out dx, al
    mov eax, esi

;;No2: Load LBA address
;   LBA 7~0 to 0x1f3
    mov dx, 0x1f3
    out dx,al

;LBA 15~8 to 0x1f4
    mov cl, 8
    shr eax, cl
    mov dx, 0x1f4
    out dx, al

;LBA 23~16 to 0x1f5
    shr eax, cl
    mov dx, 0x1f5
    out dx, al

    shr eax, cl
    and al, 0x0f
    or  al, 0xe0
    mov dx, 0x1f6
    out dx, al

;;No3 read 
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

;;No4 test disk status
.not_ready:
    nop
    in al, dx
    and al, 0x88
    cmp al, 0x08
    jnz .not_ready

;;No5: read data from 0x1f0     
    mov ax, di
    mov dx, 256
    mul dx
    mov cx, ax
    mov dx, 0x1f0
.go_on_read:
    in ax, dx
    mov [bx], ax
    add bx, 2
    loop .go_on_read
    ret

msg:
    DB 0x0a, 0x0a    ; Two line breaks
    DB "load error"  ; Error message
    DB 0x0a         ; Line break
    db 0             ; Null terminator

testmsg:
    DB 0x0a, 0x0a    ; Two line breaks
    DB "zmzmzmz"     ; Test message
    DB 0x0a         ; Line break
    db 0             ; Null terminator


resb 0x1fe-($-$$)
; times 510-($-$$) db 0
Signature_word db 0x55, 0xaa
; db 0x55 0xaa
;    low  high
; dw 0xaa55
;      h->L
