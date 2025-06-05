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

        .bss : {
                __bss_start = .;
                *(.bss)
        }
        _end = .;

        /DISCARD/ : {
                *(.eh_frame)
                *(.got.plt)
        }

}
