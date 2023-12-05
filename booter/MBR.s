org 0x7c00
%include "boot.inc"
; mov eax, 2  ;; LOADER_START_SECTOR
; mov bx, LOADER_BASE_ADDR
; ; mov cx, 10
; mov cx, 2
; ; call ide_identify
; call read_hard_disk_16
; jmp LOADER_BASE_ADDR


xor ax, ax             ; Ensure data & extra segments are 0 to start, can help
mov es, ax             ; with booting on hardware
mov ds, ax     
;; Read in the boot block and the superblock to memory for 2nd stage bootloader
mov al, 15          ; 8 - 1 for boot block - bootsector & 8 for super block
mov bl, al
dec bl              ; BX = # of sectors to read - 1
mov di, 0xC400       ; Read sectors to this address, 7C00h-8C00h = boot block, 8C00h-9C00h = superblock

mov dx, 1F6h        ; Head / drive port, flags
mov al, 0xE0        ; 0b0111 0000, bits: 0-3 = LBA bits 24-27, 4 = drive, 5 = always set, 6 = set for LBA, 7 = always set
out dx, al

mov dx, 1F2h        ; Sector count port
mov al, 15          ; # of sectors to read
out dx, al

inc dx              ; 1F3h, sector # port / LBA low
mov al, 2           ; LBA 1 = 2nd disk sector (0-based), bits 0-7 of LBA
out dx, al

inc dx              ; 1F4h, Cylinder low / LBA mid
xor ax, ax          ; bits 8-15 of LBA
out dx, al

inc dx              ; 1F5h, Cylinder high / LBA high
; xor ax, ax        ; bits 16-23 of LBA
out dx, al

inc dx
inc dx              ; 1F7h, Command port
mov al, 20h         ; Read with retry
out dx, al

call load_sector_loop

jmp LOADER_BASE_ADDR

ide_identify:
    xchg bx, bx
    mov dx, 0x1f6 ; device register port
    mov eax, 0x0f
    out dx, al

    mov dx, 0x1f7; command port
    mov eax, 0xEC; to identify device
    out dx, al   ;
    ;;No4 test disk status
    .not_ready:
        in al, dx
        and al, 0x88
        cmp al, 0x08
        jnz .not_ready

    ;;No5: read data from 0x1f0     
        mov dx, 0x1f0
        mov bx, 0x9000
        mov cx, 512
    .go_on_read:
        in ax, dx
        mov [bx], ax
        add bx, 2
        dec cl
        jnz .go_on_read
        ret

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
    test al, 8
    ; and al, 0x88
    ; cmp al, 0x08
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

;; di : address to read into.
;; 
;; Poll status port after reading 1 sector
load_sector_loop:
    in al, dx           ; Status register (reading port 1F7h)
    test al, 8          ; Sector buffer requires servicing 
    je load_sector_loop ; Keep trying until sector buffer is ready

    mov cx, 256         ; # of words to read for 1 sector
    mov dx, 1F0h        ; Data port, reading 
    rep insw            ; Read bytes from DX port # into DI, CX # of times
    
    ;; 400ns delay - Read alternate status register
    mov dx, 3F6h
    in al, dx
    in al, dx
    in al, dx
    in al, dx

    cmp bl, 0           ; Sector # to still read
    je .return

    dec bl
    mov dx, 1F7h
    jmp load_sector_loop

.return:
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
