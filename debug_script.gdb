target remote localhost:1234

file core_symbol.img

# b bootpack.c:114
# b systask.c:12

# b msg_receive
# b msg_send
# b sys_sendrec
# b task_sys
# b u_funf

# b UkiMain
# b identify_disk
# b bootpack.c:99
# b bootpack.c:99
# b read_dpt
# b ide.c:338
# b next_dpt
b read_all_dpt
b ide.c:464
# b get_dpt
# b enable_mouse
# b ide_init
# b bootpack.c:106
# b ide_read
# b int.c:89
# b protect.c:114
# b exception_handler
tui en
c

