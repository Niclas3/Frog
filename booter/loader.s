%include "boot.inc"
; section loader vstart=LOADER_BASE_ADDR ;0xc400
org 0xc400
jmp loader_start

LOADER_STACK_TOP  equ  LOADER_BASE_ADDR

section .gstack
align 8
GLOBAL_STACK:
    times 256 db 0
GLOBAL_STACK_TOP equ $


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
; CALL_GATE_DESC dw  (CALL_GATE_TEST & 0xFFFF)
;                dw  SELECTOR_RING0_C_C0DE
;                dw  CALL_GATE_HIGH2
;                dw  ((CALL_GATE_TEST>> 16) & 0xFFFF)

;; No.6 
;;                           base , limit, attr
; CODE_DESC_RING3    Descriptor 0    , code_in_ring3_len -1   , 0x4000+0x98+60h
;; No.7 
; STACK_DESC_RING3   Descriptor 0    , RING3_STACK_LIMIT , 0x93+0x4000+0x60

;; No.8
; TSS_DESC             Descriptor 0    , TSS_Len-1, 0x89

;; No.9
; C_RING0_CODE_DESC  dd  0x0000FFFF
;                    dd  DESC_C_CODE_HIGH4

;; No.10
; STACK_DESC_GLOBLE  dd  0x0000FFFF
;                    dd  DESC_DATA_HIGH4

times 10 dq 0  ;; dq for 8 bytes
;;END GDT
;;Q: How many gdt descriptor that I need?
;;A: Maybe for now , I need 4 descriptors, ring0 2, and ring3 2 for code and
;    data

;;------------------------------------------------------------
;; For gdtr 32-bit + 16-bit
;;          base     limit
GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1
;;------------------------------------------------------------

;;; Selector
;;; +----------------------------------------------+
;;; |      15-3                     |   2  |  1-0  |
;;; |descriptor index               |  TI  |  RPL  |
;;; +----------------------------------------------+
;;; TI == 0 in GDT
;;; TI == 1 in LDT
SELECTOR_CODE        equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA        equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO       equ (0x0003<<3) + TI_GDT + RPL0
SELECTOR_VGC         equ (0x0004<<3) + TI_GDT + RPL0
; SELECTOR_CALL_GATE   equ (0x0005<<3) + TI_GDT + RPL0
; SELECTOR_CODE_RING3  equ (0x0006<<3) + TI_GDT + RPL3
; SELECTOR_STACK_RING3 equ (0x0007<<3) + TI_GDT + RPL3
; SELECTOR_TSS         equ (0x0008<<3) + TI_GDT + RPL0
; SELECTOR_RING0_C_C0DE equ (0x0009<<3) + TI_GDT + RPL0
; SELECTOR_STACK       equ (0x000a<<3) + TI_GDT + RPL0

gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

;;--------------------------------------------------------
;;                      IDT
;;--------------------------------------------------------
section .idt
; [bits 32]
IDT_BASE:
%rep 255
         dw  (Dumb_handler& 0xFFFF)
         dw  SELECTOR_CODE
         dw  INTR_GATE_HIGH2
         dw  ((Dumb_handler >> 16) & 0xFFFF)
%endrep

IDT_SIZE equ $$-IDT_BASE
IDT_LIMIT equ IDT_SIZE - 1

idt_ptr  dw IDT_LIMIT
         dd IDT_BASE

;; END IDT
;;--------------------------------------------------------

;;--------------------------------------------------------
;; Real mode code (aka 16-bits codes)
;;--------------------------------------------------------
section .s16
[bits 16]
loadermsg: db 'loader in real.'
           db 0
.len equ ($-loadermsg)

print_string:
    mov ah, 0eh
    .ps_loop:
        lodsb
        int 10h
    loop .ps_loop
    ret

loader_start:

    ; mov al,0x03
    ; mov ah,0x00
    ; int 0x10
    mov si, loadermsg
    mov cx, loadermsg.len
    call print_string
    mov ah, 0x0e
    mov al, 'z'
    int 0x10

;-----------------------------------------------------------
;  print message 
;-----------------------------------------------------------
; print_begin:
;     mov ax, loadermsg
;     mov si, ax
; _print_begin:
;     mov al,[si]
;     add si,1
;     cmp al,0
;     je rst_b_scr
;
;     mov ah, 0x0e
;     mov bx, 15
;     int 10h
;     jmp _print_begin
;-----------------------------------------------------------

;----------------------------------------------------
; VBE setting
;----------------------------------------------------
;; test if support VBE 
xchg bx, bx
mov ax, 0x9000
mov es, ax
mov di, 0
mov ax, 0x4f00
int 0x10
cmp ax, 0x004f
jne scr_320
;; test VBE version is over 2.0
mov ax, [es:di+4]
cmp ax, 0x0200
jb scr_320 ;if(ax < 0x200) goto scr_320
VBEMODE  equ 0x105
mov cx, VBEMODE
mov ax, 0x4f101
int 0x10
cmp ax, 0x004f
jne scr_320
;; test VBE color mode
cmp byte [es:di+0x19], 8
jne scr_320
cmp byte [es:di+0x1b], 4
jne scr_320
mov ax, [es:di+0x00]
and ax, 0x0080
jz scr_320

switch_screen_mode:
    mov bx, VBEMODE + 0x4000
    mov ax,0x4f02
    int 0x10
    mov byte [VMODE], 8
    mov ax, [es:di+0x12]
    mov [SCRNX], ax
    mov ax, [es:di+0x14]
    mov [SCRNY], ax
    mov eax, [es:di+0x28]
    mov [VRAM], eax
    jmp keystatus

scr_320:
    mov al,0x13
    mov ah,0x00
    int 0x10
    mov byte [VMODE],8
    mov word [SCRNX],320
    mov word [SCRNY],200
    mov dword [VRAM],0x000a0000

keystatus:
    nop
;----------------------------------------------------

;----------------------------------------------------
;                  A20 open
;----------------------------------------------------
in al, 0x92
or al, 0000_0010b
out 0x92, al

;-----------------------------------------------------
;                  load gdt to gdtr
;-----------------------------------------------------
lgdt [gdt_ptr]
;-----------------------------------------------------
;                  cr0 set PE 
;-----------------------------------------------------
mov eax, cr0
or  eax, 0x00000001
mov cr0, eax
;------------------init idt---------------------------
cli
lidt [idt_ptr]
;-----------------------------------------------------
;  Jump to protected mode
;-----------------------------------------------------
jmp dword SELECTOR_CODE: LABEL_SEG_CODE32; reflash code-flow?
;-----------------------------------------------------

section .s32
bits 32
LABEL_SEG_CODE32:
    mov ax, SELECTOR_DATA   ;;SELECTOR_STACK
    mov ss, ax

    xor ax, ax
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax

    mov esp, GLOBAL_STACK_TOP ; loader stack
    mov ax, SELECTOR_VGC
    mov gs, ax

;;==============================================================================
; Paging
    call setup_page

    mov eax, PAGE_DIR_START
    mov cr3, eax

    ;open cr0 page bit
    mov eax, cr0
    or eax, 0x8000_0000
    mov cr0, eax
;--------------------------------------------------------
; Change GDT position
    sgdt [gdt_ptr]

    ;TODO: should I change to new base address
    ; set video descriptor new base
    mov ebx, [gdt_ptr+2]
    ;Change gdt base to 0xc00XX_XXXX
    or ebx, 0xc000_0000
    mov [gdt_ptr+2], ebx
    ; ;             No.x * 8
    ; or dword [ebx+0x18+4], 0xc000_0000
    ;
    ; ; set VGA descriptor new base
    ; or dword [ebx+0x20+4], 0xc000_0000
    ;
    ; ; set Data descriptor new base 
    ; or dword [ebx+0x10+4], 0xc000_0000

    ; set code descriptor new base 
    ; or dword [ebx+0x8+4], 0xc000_0000

    ; add esp, 0xc000_0000
    lgdt [gdt_ptr]

;;==============================================================================
;; load kernel.elf to 0x90000 ~ 0xA4000 80kb
;;==============================================================================
    KERNELBIN_START equ 0x95000
    KERNEL_START    equ 0xc0080000

; eax = LBA sector number
; ebx  = base address 
; ecx  = read-in sector number
    ; mov eax, 13
    ; mov ebx, KERNELBIN_START
    ; ; mov ecx, 30  ; for 15kb
    ; ; mov ecx, 40  ; for 20kb
    ; ; mov ecx, 128 ; for 64kb
    ; mov ecx, 160 ; for 80kb
    ; call SELECTOR_CODE:read_hard_disk_32

    ; Read n sector from hard disk 
    ; ebp+4 ---> count read-in sector number
    ; ebp+8 ---> base address
    ; ebp+12---> LBA sector number
    push 160
    push KERNELBIN_START
    push 13
    call SELECTOR_CODE:read_hard_disk_qemu
    add esp, 12   ; Clean up the stack after the function call

;;==============================================================================
;; Break down kernel from 0x90000 ~ ? to 0x80000
;;==============================================================================
    ;;1. Read ELF header, find .text segment
    ; #define EI_NIDENT 16                      offset     ,length
    ; typedef struct {                      0x7f,elf, 1 byte  , 1 byte , 2 bytes    8bytes
    ;     unsigned char e_ident[EI_NIDENT];// EI_MAG, EI_CLASS, EI_DATA, EI_VERSION
                                          ;//   8  bytes    ,8 bytes zero
    ;     uint16_t      e_type;            //   16 bytes    ,2 bytes
    ;     uint16_t      e_machine;         //   18 bytes    ,2 bytes 
    ;     uint32_t      e_version;         //   20 bytes    ,4 bytes 
    ;     ElfN_Addr     e_entry;           //   24 bytes    ,4 bytes  ;Address where execution starts
    ;   * ElfN_Off      e_phoff;           //   28 bytes    ,4 bytes
    ;   * ElfN_Off      e_shoff;           //   32 bytes    ,4 bytes
    ;     uint32_t      e_flags;           //   36 bytes    ,4 bytes
    ;     uint16_t      e_ehsize;          //   40 bytes    ,2 bytes
    ;   * uint16_t      e_phentsize;       //   42 bytes    ,2 bytes
    ;   * uint16_t      e_phnum;           //   44 bytes    ,2 bytes
    ;   * uint16_t      e_shentsize;       //   46 bytes    ,2 bytes
    ;   * uint16_t      e_shnum;           //   48 bytes    ,2 bytes
    ;     uint16_t      e_shstrndx;        //   50 bytes    ,2 bytes
    ; } ElfN_Ehdr;
    ; Program header
    ; typedef struct {
    ;     uint32_t      p_type;            //   0  byte     ,4 bytes
    ;     Elf32_Off     p_offset;          //   4  bytes    ,4 bytes
    ;     Elf32_Addr    p_vaddr;           //   8  bytes    ,4 bytes
    ;     Elf32_Addr    p_paddr;           //   12 bytes    ,4 bytes
    ;     uint32_t      p_filesz;          //   16 bytes    ,4 bytes
    ;     uint32_t      p_memsz;           //   20 bytes    ,4 bytes
    ;     uint32_t      p_flags;           //   24 bytes    ,4 bytes
    ;     uint32_t      p_align;           //   28 bytes    ,4 bytes
    ; } Elf32_Phdr;
    HEADER_NAME_TEXT   equ 0x0000000b
    ELF_BASE           equ 0x95000
    E_PHOFF_OFFSET     equ 28   ; 4 bytes
    E_PHENTSIZE_OFFSET equ 42   ; 2 bytes
    E_PHNUM_OFFSET     equ 44   ; 2 bytes

    ; BASE_OF_SECTION = KERNELBIN_START + eax
    ; int HEADER_NAME_TEXT = 0x0000000b
    ; void copymem(int baseaddress, int size, int desaddress);
    ; char *ELF_BIN_BASE = 0x90000;
    ; char program_table[4] = elf_base[E_PHOFF_OFFSET]; // phoff_idx = 28
    ; char program_number[2] = elf_base[E_PHNUM_OFFSET]; // phnum_idx = 44
    ; char program_entry_sz[4] = elf_base[E_PHENTSIZE_OFFSET]; // phentsize = 42
    ; for(int i=0;i < program_number;i++){
    ;     char *entry = program_table + (i*program_entry_sz);
    ;     char *p_offset[4] = entry[4];
    ;     char *p_vaddr[4]  = entry[8];
    ;     char *p_filez[4]  = entry[16];
    ;     copymem( ELF_BASE + p_offset , p_filesz, p_vaddr);
    ; }
    mov dword eax, [KERNELBIN_START+E_PHOFF_OFFSET]   ; program table offset
    mov word cx,   [KERNELBIN_START+E_PHNUM_OFFSET]   ; program entry number
    mov word bx,   [KERNELBIN_START+E_PHENTSIZE_OFFSET]   ; program table size 
    and ecx, 0x0000ffff
    and ebx, 0x0000ffff

;void load_program(program_table, count, program_size)
    ;; ebp+8 ---> size : program_entry_size
    ;; ebp+12 ---> count: number of program headers
    ;; ebp+16---> offset: start of program headers aka program_table
    push eax
    push ecx
    push ebx
    call load_program
    add esp, 12   ; Clean up the stack after the function call
;;==============================================================================
    ;Load font.img
    ; load font start at FONT_START
    FONT_START equ 0x96000
; ;; eax = LBA sector number
; ;; ebx  = base address 
; ;; ecx  = read-in sector number
;     mov eax, 173           ; according to makefile `mount` seek option
;     mov ebx, FONT_START
;     mov ecx, 8            ; all size = 4K  = 8 * 512
;     call SELECTOR_CODE:read_hard_disk_32

    ; Read n sector from hard disk 
    ; ebp+4 ---> count read-in sector number
    ; ebp+8 ---> base address
    ; ebp+12---> LBA sector number
    push 8
    push FONT_START
    push 173
    call SELECTOR_CODE:read_hard_disk_qemu
    add esp, 12   ; Clean up the stack after the function call

;;-----------------------------------------------------------------------------
    ; mov esp, 0x80000  ;set kernel stack
    mov esp, 0xc009f000  ;set kernel stack
    jmp dword SELECTOR_CODE: KERNEL_START

;===============================================================================
; dumb handler for interrupt handler 
; (just for place holder)
;===============================================================================
_Dumb_handler:
Dumb_handler equ _Dumb_handler - $$
    iretd

;===============================================================================
; Paging
; void setup_page()
;===============================================================================
setup_page:
    ; size   counts
    ;   4b x  1024 = 4096d aka 0x1000
    ; It is size of all entries.
    ; To clear 4096 byte memory from PAGE_DIR_START.
    ; This is for page directory !
;  ebp+4 ---> size in byte
;  ebp+8 ---> start_addr
;; clear up 5 * 4KB size memory for 1 PDT and 4 PT like linux

;     mov ecx, 4096
;     mov esi, 0
; .clear_page_dir:
;     mov byte [PAGE_DIR_START+esi],0
;     inc esi
;     loop .clear_page_dir

    push PAGE_DIR_START  ;;push 0x100000
    push 4096 * 9        ;;size of 1 page dir table + 8 page tables
    call clearmem
    add esp, 8

;;Create PDE (page directory entry) size 4 bytes
    mov eax, PAGE_DIR_START
    add eax, 0x1000; First page address, because of page directory size is 1000h
    mov ebx, eax   ; Base address of first page table to ebx
;; First entry of page dir
;; 0010 1000
    or eax, PG_US_U | PG_RW_W | PG_P ; eax contains page address + attributes
    mov [PAGE_DIR_START+0x0], eax ; Set first pde about 4M physical memory
;; Set 0xc00 entry of page dir 
;; No.768 dir entry of table
;; 0xc00 upper for kernel as
;; table 0xc000_0000 ~ 0xffff_ffff all 1G size for kernel
;;       0x0000_0000 ~ 0xbfff_ffff all 3G size for user
    mov [PAGE_DIR_START+0xc00],eax
;;The last entry point to it self
    sub eax, 0x1000
    mov [PAGE_DIR_START+4092], eax

; Page table entry
; 1M is what I want to used.
; 4K is one page size represents memory size,
; so as one page table entry represents memory size
; 1M / 4K = 256
; ----------------------------------------------
; 2M / 4K = 512
; 3M / 4K = 768
; 4M / 4K = 1024  This max

;Page table manage size 
PG_MSIZE_256K equ 64
PG_MSIZE_512K equ 128
PG_MSIZE_1M   equ 256
PG_MSIZE_2M   equ 512
PG_MSIZE_3M   equ 768
PG_MSIZE_4M   equ 1024

;First page  .pg0
    mov ecx, PG_MSIZE_1M
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
    mov [ebx+esi*4], edx ; ebx: start_of_pagetable, 4: 4 bytes one entry
    add edx, 4096        ; 4096b=0x1000=4b*1024 size of one page
    inc esi
    loop .create_pte

;;-------------------------------------------------------
; Other 7 PDEs
; 2nd page table address
; int_32 pt_address = 0x2000;
; int_32 pde = pt_address | PG_US_U | PG_RW_W | PG_P;
;;-------------------------------------------------------
;     mov eax, PAGE_DIR_START
;     add eax, 0x2000
;     or  eax, PG_US_U | PG_RW_W | PG_P
;
;     mov ebx, PAGE_DIR_START
;     mov ecx, 7 ; 7 page tables represent for 7 * 4M
;     mov esi, 1 ; the first page tables has already set
; .create_pde:
;     mov [ebx+esi*4], eax
;     inc esi
;     add eax, 0x1000
;     loop .create_pde
;;-----------------------------------------------------

;Second page  .pg1
;     mov ebx, PAGE_DIR_START
;     add ebx, 0x2000
;     mov ecx, 1024
;     mov esi, 0
;     mov edx, PG_US_U | PG_RW_W | PG_P
; .create_pg1_pte:
;     mov [ebx+esi*4], edx ; ebx: start_of_pagetable, 4: 4 bytes one entry
;     add edx, 4096        ; 4096b=0x1000=4b*1024 size of one page
;     inc esi
;     loop .create_pg1_pte

    mov eax, PAGE_DIR_START
    add eax, 0x2000
    or  eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_START
    mov ecx, 254
    mov esi, 769
.create_kernel_pde:
    mov [ebx+esi*4], eax ; no.769 ~ no.1023 pde -> 2nd page address
    inc esi
    add eax, 0x1000 ;4096 = size page table
    loop .create_kernel_pde

    ret

;===============================================================================
;void setpage(start_addr, count)
; ebp+4 --> count    e.g 1024
; ebp+8 --> start_addr e.g ebx
;===============================================================================

setpage:
    mov ecx, [ebp+4]
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
.c_pte:
    mov ebx, [ebp+8]
    mov [ebx+esi*4], edx ; ebx: start_of_pagetable, 4: 4 bytes one entry
    add edx, 4096 ; 4096b=0x1000=4b*1024 size of one page
    inc esi
    loop .c_pte
    ret

;===============================================================================
; void read_hard_disk_qemu (LBA_sector_number, base_address, count_sector)
; Read n sector from hard disk 
;; ebp+8  ---> LBA sector number
;; ebp+12 ---> base address
;; ebp+16 ---> count read-in sector number
;===============================================================================
read_hard_disk_qemu:
    mov ebp, esp
    ;; Read
    mov edi, [ebp+12]         ; Read sectors to this address
    mov ebx, [ebp+16]         ; count of sectors to read 

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
    mov al, [ebp+16]         ; of sectors to read
    out dx, al

    inc dx              ; 1F3h, sector # port / LBA low
    mov eax, [ebp+8]     ; LBA 1, bits 0-7 of LBA
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

;===============================================================================
; void read_hard_disk_32(sector_number, base_address,count_of_read-in_sector)
; Read n sector from hard disk 
;; eax = LBA sector number
;; ebx  = base address 
;; ecx  = read-in sector number
;===============================================================================
read_hard_disk_32:
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
    out dx, al

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
    mov [ebx], ax
    add ebx, 2
    loop .go_on_read
    ret

;===============================================================================
;void load_text_sections(section_offset, count, size)
;; ebp+4 ---> size   0x0028          ebx 3
;; ebp+8 ---> count  0x0007          ecx 2
;; ebp+12---> section_offset 0x3070  eax 1
;===============================================================================
load_text_sections:
    mov ebp, esp
    ; Getting the values of section_offset array
    mov eax, ELF_BASE
    add eax, [ebp+12]
    mov ebx, eax        ;Start of section

; Initializing loop counter
    mov esi, 0
section_loop:
    mov edx, [ebp+4]    ; Size of section entry
    imul edx, esi ; (size * i)
    add  edx, ebx
    mov eax, [edx]; name start pointer 
    push eax            ;name string pointer [ebp-4]
    ;-------------------------------------------
    xor eax, eax
    mov eax, [edx +16]   ; offset 2 byte
    push eax                   ; [ebp-8]
    ;--------------------------------------------
    xor eax, eax
    mov eax, [edx +20]   ; section size 4 byte
    push eax                   ; [ebp-12]
    ;-------------------------------------------
    ;     if(name == HEADER_NAME_TEXT){
    ;         copymem(elf_base+sh_offset,size, 0x80000 )
    ;     }
    ; Compare the name with HEADER_NAME_TEXT (0x0000000b)
    cmp dword [ebp-4], HEADER_NAME_TEXT
    jne skip_copymem
    mov eax, [ebp-8]
    add eax, ELF_BASE  ;eax -> source address

    push eax
    ; push 0x4000        ; The size of code you want to load
    ; push 0x5000        ; The size of code you want to load
    ; push 0x6000        ; The size of code you want to load
    push 0x8000        ; The size of code you want to load
    push KERNEL_START  ;0x80000 -> target address

    ; Call copymem(elf_base + sh_offset, size, 0x80000)
    call copyMem
    add esp, 12   ; Clean up the stack after the function call
    add esp, 12   ; Clean all local var stack up
    ret
skip_copymem:
    ;-------------------------------
    mov eax, esp  ;pop 3 useless data
    add eax, 12
    mov esp, eax
    ;-------------------------------
    mov eax, esi
    inc eax
    mov esi, eax
    mov ecx, [ebp+8]
    cmp esi, ecx
    jl section_loop
    ret

;===============================================================================
;void load_program(program_table, count, program_size)
;; ebp+8 ---> size : program_entry_size
;; ebp+12 ---> count: number of program headers
;; ebp+16---> offset: start of program headers aka program_table
;===============================================================================
; void copymem(int baseaddress, int size, int desaddress);
; char *ELF_BIN_BASE = 0x90000;
; int program_entry_sz = program_size / program_number;
; for(int i=0;i < program_number;i++){
;     char *entry = program_table + (i*program_entry_sz);
;     char *p_offset[4] = entry[4];
;     char *p_vaddr[4]  = entry[8];
;     char *p_filez[4]  = entry[16];
;     char *p_type[4] = entry[0]
;     if(p_type == PT_LOAD){ // PT_LOAD == 0x10000000
;       copymem( ELF_BASE + p_offset , p_filesz, p_vaddr);
;     }
; }
load_program:
    push ebp
    mov ebp, esp
    xor esi, esi
    mov ecx, [ebp+12] ; load number of entry
.load_program_start:
    mov edx, 32      ; size of each entry is 0x1c but offset is 0x20
    imul edx, esi    ; entry offset
    mov ebx, [ebp+16]; program_table offset from base
    add ebx, KERNELBIN_START
    add ebx, edx     ; entry address
    mov eax, [ebx+0] ; p_type
    cmp eax, 0x1
    jnz .load_program_continue

    mov eax, [ebx+4] ; p_offset, size 4 bytes
    add eax, KERNELBIN_START; set elf base address from 0x0 to 0x90000
    push eax         ; source of start
    mov eax, [ebx+16]; p_filez,  size 4 bytes
    push eax         ; size of data we want to copy
    mov eax, [ebx+8] ; p_vaddr,  size 4 bytes
    push eax         ; dest of copy
    call copyMem
    add esp, 12   ; Clean up the stack after the function call
    inc esi
.load_program_continue:
    loop .load_program_start
    pop ebp
    ret
;===============================================================================
; void copymem(int baseaddress, int size, int desaddress);
;; ebp+20 ---> destination  0x80000          ebx 3
;; ebp+24 ---> size                        ecx 2
;; ebp+28---> baseaddress                     1
;===============================================================================
copyMem:
    ; Implement the copymem function here
    ; This function should copy 'size' bytes from 'baseaddress' to 'desaddress'
    ; You can use the 'movsb' instruction for byte-by-byte copying
    push ebp
    push esi
    push edi
    push ecx
    mov ebp, esp
    mov esi, [ebp+28] ; base address
    mov edi, [ebp+20]  ; destination
    mov ecx, [ebp+24]  ; size
    cld
    rep movsb
    pop ecx
    pop edi
    pop esi
    pop ebp
    ret

;===============================================================================
; void clearmem(start_addr, size)
;     ebp+4 ---> size in byte
;     ebp+8 ---> start_addr
;===============================================================================
clearmem:
    mov ebp, esp
    mov ecx, [ebp+4]
    mov esi, 0
.clear_mem:
    mov eax, [ebp+8]
    mov byte [eax + esi],0
    inc esi
    loop .clear_mem
    ret

