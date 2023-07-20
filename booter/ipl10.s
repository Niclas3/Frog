org 0x7c00
CYLS equ 10
;;BIOS will load this 512 bytes and execute.
;------------------------------------------------------------------------------- 
;              BPB (BIOS parameter block) structure
;-------------------------------------------------------------------------------
; Jump insterction to boot code. This field has two allowed forms:
; 
; jmpBoot[0] = 0xEB   ; jmp 
; jmpBoot[1] = 0x??   ; "some label"
; jmpBoot[2] = 0x90   ; nop
; 
; and 
; 
; jmpBoot[0] = 0xE9
; jmpBoot[1] = 0x??
; jmpBoot[2] = 0x??
;
; 0x?? indicates that any 8-bit value is allowed in that byte. What this forms
; is a three-byte Intel x86 unconditional branch (jump) instruction that jumps
; to the start of the operating the rest of sector 0 of the volume following the
; BPB and possibly other sectors. Either of these forms is acceptable.
; jmpBoot[0] = 0xEB is the more frequently used format.
;
; e.g. jmp          0xeb
;      LABEL_START  0x**
;      nop          0x90
; `nop` is needed
;
  ; BS_jmpBoot db  0xeb, 0x4e, 0x90
  jmp short entry
  nop
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; OEM name identifier. can be set by a FAT implementation to any desired value.|
; 
  BS_OEMName db "HELLOIPL"
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Count of bytes per sector. this value may take on only the following values. |
; 512, 1024, 2048 or 4096                                                      |
; 
  BPB_BytsPerSec dw 512
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Number of sectors per allocation unit.This value must be a power of 2 that is|
; greater than 0.
; THE LEGAL values are, 1,2,4,8,16,32,64, and 128.
; 
  BPB_SecPerClus db 1
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Number of reserved sectors in the reserved region of the volume starting at
; the first sector of the volume. This field is used to align the start of the
; data area to integral multiples of the cluster size with respect to the start
; of the partition/media.
; The field must not be 0 and can be any non-zero value.
; The field should typically be used to align the start of the data area
; (cluster #2) to the desired alignment unit, typically cluster size.
;
  BPB_RsvSecCnt dw 1
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; The count of file allocation tables(FATs) on the volume. A value of 2 is
; recommended although a value of 1 is acceptable
; it means that this FAT12 floppy has FAT1 and FAT2.
;
  BPB_NumFATs db 2
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; For FAT12 and FAT16 volumes, this field contains the count of 32-byte
; directory entries in the root directory. For FAT32 volumes, this field must be
; set to 0. For FAT12 and FAT16 volumes, this value should always specify a
; count that when multiplied by 32 results in an even multiple of
; BPB_BytsPerSec.
; if you want to mount it at ubuntu. a directory entry has 64-byte size.
; lower 32-byte is called slot for containing more file name.
; struct slot {
;     unsigned char id;           // sequence number for slot
;     unsigned char name0_4[10];  // frist 5 characters in name
;     unsigned char attr;         // attribute byte
;     unsigned char reserved;     // always 0
;     unsigned char alias_checksum; // checksum for 8.3 alias
;     unsigned char name5_10[12]; // 6 more characters in name
;     unsigned char start[2];     // starting cluster number
;     unsigned char name11_12[4]; // last 2 characters in name
; }
; 
; For maximum compatibility, FAT16 volumes should use the value 512.
;
  BPB_RootEntCnt dw 224
  ; BPB_RootEntCnt dw 10
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; This field is the old 16-bit total count of sectors on the volume. This count
; includes the count of all sectors in all four regions of the volume.
;
; !This field can be 0; if it is 0, then BPB_TotSec32 must be non-zero.
; 
; For FAT12 and FAT16 volumes, this field contains the sector count, and
; BPB_TotSec32 is 0 if the total sector count "fits" (is less than 0x10000)
;
  BPB_TotSec16 dw 2880
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; The legal values for this field are
; 0xF0, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, and 0xFF.
; 
; 0xF8 is the standard value for "fixed"(non-removable) media. For removable
; media, 0xF0 is frequently used.
; 
 BPB_Media db 0xf0
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; This field is the FAT12/FAT16 16-bits count of sectors occupied by one FAT. On
; FAT32 volumes this field must be 0, and BPB_FATSz32 contains the FAT size
; count.
;
 BPB_FATSz16 dw 9
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Sectors per track for interrupt 0x13.
; 
; This field is only relevant for media that have a geometry(volume is broken
; down int tracks by multiple heads and cylinders) and are visble on interrupt
; 0x13.
;
 BPB_SecPerTrk dw 18
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Number of heads for interrupt 0x13. This field is relevant as discussed
; earlier for BPB_SecPerTrk
; 
; This field contains the one based "count of heads".
; For example, on a 1.44MB 3.5-inch floppy drive this value is 2.
;
 BPB_NumHeads dw 2
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Count of hidden sectors preceding the partition that contains this FAT
; volume.This field is generally only relevant for media visible on interrupt
; 0x13.
; 
; This field must always be zero on media that are not partitioned.
;
; NOTE: Attempting to utilize this field to align the start of data area is
; incorrect
;
 BPB_HiddSec dd 0
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; This field is the new 32-bit total count of sectors on the volume. This count
; includes the count of all sectors in all four regions of the volume.
; 
; This field can be 0; if it is 0, Then BPB_TotSec16 must be non-zero. For
; FAT12/FAT16 volumes, this field contains the sector count if BPB_TotSec16 is 0
; (count is greater than or equal to 0x10000).
;
; For FAT32 volumes, this field must be non-zero.
;
 BPB_TotSec32 dd 0
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Interrupt 0x13 drive number. Set value to 0x80 or 0x00
;
 BS_DrvNum db 0
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Reserved.Set value to 0x0
; 
 BS_Reserved1 db 0
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Extended boot signature. Set value to 0x29 if either of the following two
; fields are non-zero.
; 
; This is a signature byte that indicates that the following three fields in the
; boot sector are presents.
;
 BS_BootSig db 0x29
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Volume serial number.
; 
; This field, together with BS_VolLab, supports volume tracking on removable
; media.these values allow FAT file system drivers to detect that the wrong disk
; is inserted in a removable drive.
;
; This ID should be generated by simply combining the current date and time into
; a 32-bit value.
;
 BS_VolID dd 0
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; Volume label. This field matches the 11-byte volume label recorded in the
; root directory.
; 
; NOTE: FAT file system drivers must ensure that they update this field when the
; volume label file in the root directory has its name changed or created. The
; setting for this field when there is no volume label is the string 
; "NO NAME    " string must be 11 bytes
 BS_VolLab DB		"TOY-OS     "
;-------------------------------------------------------------------------------
;-------------------------------------------------------------------------------
; One of the strings "FAT12   ","FAT16   ",or "FAT     ".
; 
; NOTE: This string is informational only and does not determine the FAT type.
; length must be 8 bytes
 BS_FilSysType db "FAT12   "

;-------------------------------------------------------------------------------
; We dont need this when we know that the first
; Jump insterction to boot code. This field has two allowed forms
; resb 18
;-------------------------------------------------------------------------------
BaseOfLoaderAddress equ 0x0820
entry:
    mov ax, 0
    mov ss, ax       ; Set the stack segment to 0
    mov sp, 0x7c00   ; Set the stack pointer to 0x7c00
    mov ds, ax       ; Set the data segment to 0
    mov es, ax       ; Set the extra segment to 0

; Read c0-h0-s2 sector
mov ax, BaseOfLoaderAddress; Set base address for loading data from disk to memory
mov es, ax          ; Set es to the base address
mov ch, 0           ; Set cylinder to 0
mov dh, 0           ; Set head to 0
mov cl, 2           ; Set sector to 2 the first is 1 not 0

start:
    mov si, 0        ; Retry counter

retry:
    mov ah, 0x02     ; 
    mov al, 1        ; Read 1 sector sector count (0 is illegal)
    mov bx, 0        ; Destination memory address
    mov dl, 0x00     ; Select the first driver
    int 0x13         ; BIOS interrupt for disk operations
    jnc next         ; Jump if no carry (successful read)
    add si, 1        ; Increment retry counter
    cmp si, 5
    jae error        ; Jump if retry counter exceeds 5
    mov ah, 0x00     ; Reset disk driver
    mov dl, 0x00     ; Select the first driver
    int 0x13         ; BIOS interrupt for disk operations
    jmp retry

next:
    mov ax, es
    add ax, 0x0020   ; Increment es by 512 bytes (size of sector)
    mov es, ax
    add cl, 1        ; Increment sector
    cmp cl, 18       ; Check if sector is less than or equal to 18
    jbe start        ; Jump if below or equal
    ;; let's remove head
    mov cl, 1
    add dh, 1        ; Increment head
    cmp dh, 2        ; Check if head is less than 2
    jb start         ; Jump if below

    mov dh, 0
    add ch, 1        ; Increment cylinder
    cmp ch, CYLS     ; Check if cylinder is less than CYLS (10)
    jb start         ; Jump if below

; Load directory find loader
; FirstRootDirSecNum = BPB_RsvSecCnt + (BPB_NumFATs * BPB_FATSz16)
;                      1 + (2 x 9)

; 文件的开始Sector号 = DirEntry中的开始Sector号 + 根目录占用Sector数目 + DeltaSectorNo
   ; mov bx, [BPB_NumFATs]
   ; mov word cx, [BPB_FATSz16]
   ; movzx ax, bl
   ; imul cx
   ; mov word dx, [BPB_RsvSecCnt]
   ; add dx, ax

 FirstRootDirSecNum equ 19
    mov dx, 19
;; RootDirSectors' = (BPB_RootEntCnt * 32) + (BPB_BytsPerSec-1) / BPB_BytsPerSec
;; mod == 0  RootDirSectors = RootDirSectors'
;; mod != 0  RootDirSectors = RootDirSectors' + 1
 RootDirSectors equ 15
;; RootDataSector = RootDirSectors + FirstRootDirSecNum = 34
 RootDataSector equ RootDirSectors + FirstRootDirSecNum

;;Head pointer of root directory 
; FirstRootDirSecNum * BPB_BytsPerSec + ((BaseOfLoaderAddress-20)<<4)
   mov word ax, [BPB_BytsPerSec]
   imul dx
   mov dx, BaseOfLoaderAddress
   sub dx, 0x20
   shl dx, 4
   add dx, ax

;                offset  size
; DIR_FstClusHI  20      2
; DIR_FstClusLO  26      2
; DIR_FileSize   28      2
   xor ax,ax
   mov ds,ax
   mov bx,dx
   add bx,64

   add bx, 20
   mov ax, [bx];;DIR_FstClusHI
   push ax

   add bx, 6
   mov ax, [bx];;DIR_FstClusLO
   push ax

   add bx, 2
   mov ax, [bx];;DIR_FileSize
   push ax

   ; jmp dx


   jmp 0xc400       ; Jump to OS starter?

fin:
    hlt              ; Halt the CPU
    jmp fin          ; Jump to fin (infinite loop)

error:
    mov si, msg      ; Load error message address into si

putloop:
    mov al, [si]     ; Load character from message
    add si, 1        ; Increment si
    cmp al, 0        ; Check if end of message (null terminator)
    je fin           ; Jump to fin if end of message
    mov ah, 0x0e     ; Display character in teletype mode
    mov bx, 0        ; Display page number (0)
    int 0x10         ; BIOS interrupt for video services
    jmp putloop      ; Jump to putloop for next character

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
