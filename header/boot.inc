LOADER_BASE_ADDR equ 0xc400
LOADER_START_SECTOR equ 0x2
;;------------boot information-
LEDS  equ  0x0ff1
VMODE equ  0x0ff2
SCRNX equ  0x0ff4
SCRNY equ  0x0ff6
VRAM  equ  0x0ff8
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
DESC_TYPE_DATA    equ 0010_0000_0000b

DESC_CODE_HIGH4   equ (0x00 << 24) + DESC_G_4K + \
                                     DESC_D_32 + \
                                     DESC_P + \
                                     DESC_AVL + \
                                     DESC_LIMIT_CODE2 + \
                                     DESC_DPL_0 + \
                                     DESC_S_CODE + \
                                     DESC_TYPE_CODE+ 0x00

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
                                     DESC_DPL_0 + \
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

RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b

TI_GDT  equ  000b
TI_LDT  equ  100b







