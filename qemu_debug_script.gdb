target remote localhost:1234


# file core/build/core_symbol.img
# set architecture i386
file core/apps/build/compositor
# file core/apps/build/ls
tui en
# if GDB7.0
# layout asm
# set disassemble-next-line on

# b *0xc0070000
b pc_mouse_init
# b exception_handler
# b panic_print

# b ioqueue_put_data
# b ioqueue_get_data
# b u_fund
# b u_fung

### debug compositor
b main
b poudland_move_window
# b 584
# b 711
# b window_top_of
# b send_move_window_massage
# b compositor.c:606
# b info_JPEG
# b quick_create_window
# b draw_mouse
# b poudland_blit_windows
# b bmp_meta
# b compositor.c:554



# b u_fune
# b 143
# b init_timervecs
# b timer.c:183
# b sys_char_file
# b ps2hid.c:292
# b sys_read
# b fsk_mouse.c: 52 if cursor_y == 0
# b fork.c: 65
# b sys_fork
# b u_fune
# b bootpack.c:538
# b sys_wait
# b exit.c:95
# b sys_exit
# b exit.c:37 if pg_phy_addr == 0x0
# b thread/fork.c:67 if idx_byte == 0x17000-1
# b thread/fork.c:69 if idx_bit == 0
# b load_code
# b bootpack.c: 516
# b bootpack.c: 513
# b 140

# b malloc_page_with_vaddr_test
# b sys_testsyscall
# b syscall-init.c:267
# b sys_testsyscall
# b load_elf_file
# b memory.c:107 if start_pos == 2
# b memory.c:107
# b memory.c: 455
# b sys_execv
# b load_elf_file
# b thread/exec.c:79 if ph_buf->p_offset == 0x3000
# b 555


# b file.c:559 if i == 25
# b file.c:559 if i == 31
# b file.c:559 if i == 40
# b file.c:559 if i == 10
# b 128

# b inthandler20
# b sched.c: 62 if cur_thread->ticks == 0
# b init
# b mount_partition
#
# b scan_partitions
# b 158
# b make_mouse_packet
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
