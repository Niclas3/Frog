target remote localhost:1234

# file core/build/core_symbol.img

# set architecture i386
file core/build/core_symbol.img
# b *0x7c00
tui en
# b *0xce57
# b *0xd082
b *0xc0080000
b 145
# b inthandler2C
# b ps2mouse.c: 84 if sucs == 1
# b fs_init
# b intr_hd_handler
# b ide.c:486
# b bootpack.c:120
# b draw_2d_gfx_asc_char
# b clear_screen
# b 118
# b mouse_decode
# b inthandler2C
# b mouse_consumer
# b mem_init
# b malloc_page_with_vaddr
# b fs.c:248
# b fs.c:216
# b intr_hd_handler
# b mount_partition
# b scan_partitions
# b bootpack.c:130
# b ide.c:495
# b fs.c:213
# b get_dpt
# b semaphore_down
# b ide.c:229
# b *0xc400
# c
# x/2048x 0xc400
# x/i $eip