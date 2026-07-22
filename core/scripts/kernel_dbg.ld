ENTRY(_start)

SECTIONS {
        . = 0xc0070000;
        .text : { 
                *(.text)
                *(.text.*)
        }

        .rodata : {
                __rodata_start = .;
                *(.rodata)
                *(.rodata.*)
        }

        .data : {
                __data_start = .;
                *(.data)
                *(.data.*)
                __data_end = .;
        }

        .bss (NOLOAD) : {
                __bss_start = .;
                *(.bss)
                *(.bss.*)
                *(COMMON)
        }
        _end = .;

        /*
         * The loader enters C with esp=0xc009f000.  Keep the complete
         * 0xc009e000..0xc009f000 bootstrap stack page outside PT_LOAD;
         * physical 0x000a0000 and above belongs to VGA/firmware.
         */
        ASSERT(_end <= 0xc009e000,
               "kernel PT_LOAD crosses VGA/early-stack boundary")

        /DISCARD/ : {
                *(.eh_frame)
                *(.got.plt)
        }

}
