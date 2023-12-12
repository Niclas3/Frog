org 0x7c00
%include "boot.inc"

push 11                  ; number of sectors to read for 6.5k
push LOADER_BASE_ADDR    ; target memory address
push 2                   ; read start lba address
call read_hard_disk_qemu_bit16

jmp LOADER_BASE_ADDR

ide_identify:
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

;===============================================================================
; void read_hard_disk_qemu (LBA_sector_number, base_address, count_sector)
; Read n sector from hard disk 
;; ebp +2 ---> LBA sector number
;; ebp +4 ---> base address
;; ebp +6 ---> count read-in sector number
;===============================================================================
read_hard_disk_qemu_bit16:
    mov ebp, esp
    ;; Read
    mov edi, [ebp+4]         ; Read sectors to this address
    mov ebx, [ebp+6]         ; count of sectors to read 

    mov dx, 1F6h        ; Head / drive port, flags
    mov al, 0xE0        ; 0b0111 0000,
                        ; bits: 0-3 = LBA bits 24-27, 
                        ;         4 = drive, 
                        ;         5 = always set, 
                        ;         6 = set for LBA, 
                        ;         7 = always set
    out dx, al

;;  set sector count for reading
    mov dx, 1F2h        ; Sector count port
    mov al, [ebp+6]         ; of sectors to read
    out dx, al

    inc dx              ; 1F3h, sector # port / LBA low
    mov eax, [ebp+2]     ; LBA 1, bits 0-7 of LBA
    out dx, al

    inc dx              ; 1F4h, Cylinder low / LBA mid
    mov cl, 8
    shr eax, cl
    out dx, al

    inc dx              ; 1F5h, Cylinder high / LBA high
    shr eax, cl         ; bits 16-23 of LBA
    out dx, al

    inc dx
    inc dx              ; 1F7h, Command port
    mov al, 20h         ; Read with retry
    out dx, al

    load_sector_loop:
        in al, dx           ; Status register (reading port 1F7h)
        and al, 0x88
        cmp al, 0x08          ; Sector buffer requires servicing 
        jnz load_sector_loop ; Keep trying until sector buffer is ready

        mov cx, 256         ; 2bytes 1 time. of words to read for 1 sector
        mov dx, 1F0h        ; Data port, reading 
        rep insw            ; Read bytes from EDX port # into DI, CX # of times

        ;; 400ns delay - Read alternate status register
        mov dx, 3F6h
        in al, dx
        in al, dx
        in al, dx
        in al, dx

        dec bl
        cmp bl, 0           ; Sector # to still read
        je .return

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
