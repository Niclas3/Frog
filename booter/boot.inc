;         Memory Mapping
;  fsize                memory
;───────────┌─────────┐0x0000_7C00
;     512b  │  MBR.s  │
;───────────├─────────┤0x0000_7E00
;           │         │
;   7k      │ unused  │
;           │         │
;───────────├─────────┤0x0000_9A00 (mem_bitmap_base)
;   10.5k   │         │            
;           │         │            
;           │         │            
;───────────├─────────┤0x0000_C400
;           │loader.s │
;    5k     │         │
;───────────├─────────┤0x0000_D800
;    4k     │font.img │
;───────────├─────────┤0x0000_E800
;    6k     │ unused  │
;           │         │
;───────────├─────────┤0x0001_0000
;           │         │
;  120k     │core.img │
;           │zip      │
;           │         │
;───────────├─────────┤0x0002_E000
;           │         │
;  328k     │unused   │
;           │         │
;───────────├─────────┤0x0007_0000
;           │ core    │
;  192k     │ .start  │
;           │         │
;           │         │
;───────────├─────────┤0x000a_0000 (end assume)
;           │         │
; 384k      │unused   │
;           │         │
;───────────├─────────┤0x0010_0000
;           │page     │
; 1M        │table    │
;           │         │
;           │         │
;───────────├─────────┤0x0020_0000
;           │         │
;           │         │
;           │         │
;           │         │
;───────────├─────────┤0x00a0_0000 
;                                  
;           ├─────────┤0xc006_0000 1.kernel_pool.bitmap
;           │         │            2.user_pool.bitmap
;           │         │            3.kernel_viraddr.bitmap


LOADER_BASE_ADDR equ 0xc400
LOADER_START_SECTOR equ 0x2
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
