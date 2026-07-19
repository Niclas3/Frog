OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

USER_IMAGE_BASE = 0x08048000;

SECTIONS
{
    . = USER_IMAGE_BASE;

    .text : ALIGN(16)
    {
        KEEP(*(.text._start))
        *(.text .text.*)
    }

    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4)
    {
        *(.data .data.*)
    }

    .bss : ALIGN(4)
    {
        *(.bss .bss.*)
        *(COMMON)
    }

    user_image_end = .;
    ASSERT(user_image_end - USER_IMAGE_BASE <= 0x1000,
           "user-smoke image exceeds one page")

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.got*)
    }
}
