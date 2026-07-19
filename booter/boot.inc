; =============================================================================
;                              Memory Layout
; =============================================================================
;
; A. Physical memory (low region used by boot + kernel image)
;
;   size  | region                | physical address
;  -------+-----------------------+----------------------------------
;   512 B | MBR                   | 0x0000_7C00 - 0x0000_7E00
;   ~32K  | early stack / unused  | 0x0000_7E00 - 0x0000_C400
;     5 K | loader.s              | 0x0000_C400 - 0x0000_D800
;     4 K | font.img              | 0x0000_D800 - 0x0000_E800
;     6 K | unused                | 0x0000_E800 - 0x0001_0000
;   256 K | core.img (ELF image)  | 0x0001_0000 - 0x0005_0000
;   128 K | unused                | 0x0005_0000 - 0x0007_0000
;   192 K | core (decompressed)   | 0x0007_0000 - 0x000A_0000
;         |   _end ≈ 0x0007_CFCC  |
;    64 K | VGA framebuffer       | 0x000A_0000 - 0x000B_0000  (mode 13h etc.)
;    32 K | VGA text + alt        | 0x000B_0000 - 0x000C_0000  (text @ 0xB8000)
;   256 K | BIOS ROM / reserved   | 0x000C_0000 - 0x0010_0000
;    N K  | page tables           | 0x0010_0000 - 0x0010_0000+P
;         |   (PD + PTs, sized by | (P = PAGE_SIZE * (PDT_COUNT+PGT_COUNT))
;         |    PDT_COUNT/PGT_COUNT)|
;   rest  | kernel pool / user    | above page tables;
;         | pool                  | mem_pool_init splits free RAM 50/50.
;
; Note: physical 0x000B_8000 (VGA text) lives in this region; the
; physical pool starts above 0x0020_0000 so it never gives out the
; VGA frame.  The corresponding VIRTUAL mapping at 0xC00B_8000 is what
; needs explicit reservation -- see section B.
;
;
; B. Kernel virtual address space (after mem_init runs)
;
;   virtual address      | region
;  ----------------------+-------------------------------------------------
;   0xC000_0000          | start of kernel high half (phys 0 mirrored here)
;   0xC006_0000          | MEM_BITMAP_BASE
;                        |   - kernel_pool.pool_bitmap.bits
;                        |   - user_pool.pool_bitmap.bits   (back-to-back)
;                        |   - kernel_viraddr.vaddr_bitmap.bits
;   0xC007_0000          | kernel .text (link base, kernel_dbg.ld)
;  ≈0xC007_CFCC          | _end
;  ≈0xC007_D000          | K_HEAP_START  ((_end & ~0xfff) + 0x1000)
;                        |   kernel heap grows upward; managed by
;                        |   kernel_viraddr.vaddr_bitmap; served by
;                        |   kmalloc / kfree.
;                        |
;   0xC00B_8000          | *** RESERVED *** VGA text framebuffer (1 page)
;                        |   mem_init() calls
;                        |     mark_kernel_vaddr_reserved(0xC00B8000)
;                        |   so the heap allocator never hands out this
;                        |   virtual page.  Without that reservation,
;                        |   put_page() (memory.c:152) silently rewrites
;                        |   the PTE the first time a kmalloc happens to
;                        |   land here, and writes to 0xC00B_8000 stop
;                        |   reaching physical 0xB8000.  Symptom: screen
;                        |   freezes after enough kmalloc churn (e.g.
;                        |   right after frogfs_format_partition).
;                        |
;   ... heap continues ...
;                        |
;   0xFF40_0000          | KASAN shadow memory bottom
;                        |   1 directory page + 96 shadow pages covering
;                        |   ~3 MB of real memory.  (CONFIG_POSION_MEMORY)
;   0xFF46_0000          | shadow ends
;                        |
;   0xFF80_0000          | kernel stack pool bottom
;                        |   K_THREAD_MAX (=5) * K_STACKSZ_IN_PAGE (=2)
;                        |   = 10 pages, allocated by alloc_kstack_pool()
;   0xFFBF_FFFF          | top of stack pool
;   0xFFC0_0000          | page directory + recursive page-table window
;   0xFFFF_FFFF          | top of address space
;
; =============================================================================


LOADER_BASE_ADDR equ 0xc400
LOADER_START_SECTOR equ 0x2

; Canonical disk layout. Host-side image builders mirror these values.
DISK_SECTOR_SIZE equ 512
KERNEL_START_SECTOR equ 13
KERNEL_SECTOR_COUNT equ 512
FONT_START_SECTOR equ 2048
ATA_PIO_MAX_SECTORS equ 255
;;------------boot information-
LEDS  equ  0x0ff1
VMODE equ  0x0ff2

MMAP_INFO_COUNT_POINTER equ 0x0ff4; 4 bytes
MMAP_INFO_POINTER equ  0x0ff8     ; 4 bytes

VBE_MODE_INFO_POINTER equ 0x0ffc  ; 4 bytes
VBE_INFO_POINTER equ 0x1000        ; 4 bytes

;----GDT
DESC_G_4K equ 1_000_0000_0000_0000_0000_0000b
DESC_D_32 equ 1_000_0000_0000_0000_0000_000b
DESC_L    equ 0_000_0000_0000_0000_0000_00b

DESC_AVL  equ 0_000_0000_0000_0000_0000_0b

DESC_LIMIT_CODE2  equ 1111_0000_0000_0000_0000b
DESC_LIMIT_DATA2  equ DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2 equ 0000_0000_0000_0000_000b
DESC_P            equ 1_000_0000_0000_0000b
DESC_DPL_0        equ 00_0000_0000_0000_0b
DESC_DPL_1        equ 01_0000_0000_0000_0b
DESC_DPL_2        equ 10_0000_0000_0000_0b
DESC_DPL_3        equ 11_0000_0000_0000_0b

DESC_S_CODE       equ 1_000_0000_0000_0b
DESC_S_DATA       equ DESC_S_CODE

DESC_S_SYS        equ 0_000_0000_0000_0b

DESC_TYPE_CODE    equ 1000_0000_0000b
DESC_TYPE_C_CODE    equ 1100_0000_0000b
DESC_TYPE_DATA    equ 0010_0000_0000b

;; INTER
;; 0000 0000 0000 0000 1000 0000 0000 0000  GATE_P
;; 0000 0000 0000 0000 0000 0000 0000 0000  GATE_DPL_0
;; 0000 0000 0000 0000 0010 0000 0000 0000  GATE_DPL_1
;; 0000 0000 0000 0000 0100 0000 0000 0000  GATE_DPL_2
;; 0000 0000 0000 0000 0110 0000 0000 0000  GATE_DPL_3
;; 0000 0000 0000 0000 0000 1000 0000 0000  GATE_D_32
;; 0000 0000 0000 0000 0000 0000 0000 0000  GATE_D_16
;; 0000 0000 0000 0000 0000 0110 0000 0000  GATE_TYPE_INTR_GATE
;; 0000 0000 0000 0000 0000 0111 0000 0000  GATE_TYPE_TRAP_GATE
;; 0000 0000 0000 0000 0000 0101 0000 0000  GATE_TYPE_TASK_GATE

GATE_P              equ 1000_0000_0000_0000b
GATE_DPL_0          equ 0000_0000_0000_0000b 
GATE_DPL_1          equ 0010_0000_0000_0000b 
GATE_DPL_2          equ 0100_0000_0000_0000b 
GATE_DPL_3          equ 0110_0000_0000_0000b 
GATE_D_32           equ 1000_0000_0000b 
GATE_D_16           equ 0000_0000_0000b 
GATE_TYPE_INTR_GATE equ 0110_0000_0000b 
GATE_TYPE_TRAP_GATE equ 0111_0000_0000b 
GATE_TYPE_TASK_GATE equ 0101_0000_0000b 
GATE_TYPE_CALL_GATE equ 0100_0000_0000b 

DESC_CODE_HIGH4   equ (0x00 << 24) + DESC_G_4K + \
                                     DESC_D_32 + \
                                     DESC_P + \
                                     DESC_AVL + \
                                     DESC_LIMIT_CODE2 + \
                                     DESC_DPL_0 + \
                                     DESC_S_CODE + \
                                     DESC_TYPE_CODE+ 0x00
DESC_C_CODE_HIGH4   equ (0x00 << 24) + DESC_G_4K + \
                                     DESC_D_32 + \
                                     DESC_P + \
                                     DESC_AVL + \
                                     DESC_LIMIT_CODE2 + \
                                     DESC_DPL_0 + \
                                     DESC_S_CODE + \
                                     DESC_TYPE_C_CODE+ 0x00

DESC_DATA_HIGH4   equ (0x00 << 24) + DESC_G_4K + \
                                     DESC_D_32 + \
                                     DESC_L + \
                                     DESC_AVL + \
                                     DESC_LIMIT_DATA2+ \
                                     DESC_P + \
                                     DESC_DPL_0 + \
                                     DESC_S_DATA+ \
                                     DESC_TYPE_DATA + 0x00

;; 24-bits 3-byte
DESC_VIDEO_HIGH4   equ (0x00 << 24)+ DESC_G_4K + \
                                     DESC_D_32 + \
                                     DESC_L + \
                                     DESC_AVL + \
                                     DESC_LIMIT_VIDEO2+ \
                                     DESC_P + \
                                     DESC_DPL_3 + \
                                     DESC_S_DATA+ \
                                     DESC_TYPE_DATA + 0x0b

;; 24-bits 3-byte
DESC_VGC_HIGH4   equ (0x00 << 24)+ DESC_G_4K + \
                                     DESC_D_32 + \
                                     DESC_L + \
                                     DESC_AVL + \
                                     DESC_LIMIT_VIDEO2+ \
                                     DESC_P + \
                                     DESC_DPL_0 + \
                                     DESC_S_DATA+ \
                                     DESC_TYPE_DATA + 0x0a

DESC_RING3CODE_HIGH4   equ (0x00 << 24) + DESC_G_4K + \
                                     DESC_D_32 + \
                                     DESC_P + \
                                     DESC_AVL + \
                                     DESC_LIMIT_CODE2 + \
                                     DESC_DPL_3 + \
                                     DESC_S_CODE + \
                                     DESC_TYPE_CODE+ 0x00

;; Interrupt Gate
;; high 32-bits           type
;;-------------------------------------------------------
;;|31 - 16 | 15 | 14 13 | 12  -   8 | 7  -  5 | 4  -  0 |
;;|offset  | P  |  DPL  | 0 D 1 1 0 | 0  0  0 | Reserve |
;; low 32-bits
;;|seg sel |              offset 15..0                  |
;;-------------------------------------------------------
;; D size of gate 1=32bits; 0=16bits
;; P segment present flag
;; DPL descriptor privilege level

;;
;; 0000 0000 0000 0000 0000 0000 0000 0000
;; 0000 0000 0000 0000 1000 0000 0000 0000  GATE_P
;; 0000 0000 0000 0000 0000 0000 0000 0000  GATE_DPL_0
;; 0000 0000 0000 0000 0010 0000 0000 0000  GATE_DPL_1
;; 0000 0000 0000 0000 0100 0000 0000 0000  GATE_DPL_2
;; 0000 0000 0000 0000 0110 0000 0000 0000  GATE_DPL_3
;; 0000 0000 0000 0000 0000 1000 0000 0000  GATE_D_32
;; 0000 0000 0000 0000 0000 0000 0000 0000  GATE_D_16
;; 0000 0000 0000 0000 0000 0110 0000 0000  GATE_TYPE_INTR_GATE
;; 0000 0000 0000 0000 0000 0111 0000 0000  GATE_TYPE_TRAP_GATE
;; 0000 0000 0000 0000 0000 0101 0000 0000  GATE_TYPE_TASK_GATE
;; 0000 0000 0000 0000 0000 0100 0000 0000  GATE_TYPE_CALL_GATE

INTR_GATE_HIGH2  equ (0x00 << 16)+ GATE_P + \
                                   GATE_DPL_0 + \
                                   GATE_D_32 + \
                                   GATE_TYPE_INTR_GATE

CALL_GATE_HIGH2  equ (0x00 << 16)+ GATE_P + \
                                   GATE_DPL_3 + \
                                   GATE_D_32 + \
                                   GATE_TYPE_CALL_GATE

RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b

TI_GDT  equ  000b
TI_LDT  equ  100b

;;Page 
PAGE_DIR_START equ 0x100000
; PAGE_DIR_START equ 0x0
PG_P equ 1b
PG_RW_R equ 00b
PG_RW_W equ 10b
PG_US_S equ 000b
PG_US_U equ 100b

; usage: Gate Selector, Offset, DCount, Attr
;        Selector:  dw
;        Offset:    dd
;        DCount:    db
;        Attr:      db
%macro Gate 4
	dw	(%2 & 0FFFFh)				; 偏移 1				(2 字节)
	dw	%1					; 选择子				(2 字节)
	dw	(%3 & 1Fh) | ((%4 << 8) & 0FF00h)	; 属性					(2 字节)
	dw	((%2 >> 16) & 0FFFFh)			; 偏移 2				(2 字节)
%endmacro

; usage: Descriptor addressbase, limit, attr
;        Addressbase: dd
;        limit      : dd
;        attr       : dw
%macro Descriptor 3
        dw %2 & 0xFFFF
        dw %1 & 0xFFFF
        db (%1 >> 16) & 0xFF
        dw ((%2 >> 8) & 0xF00) | (%3 & 0xF0FF)
        db (%1 >> 24) & 0xFF
%endmacro
