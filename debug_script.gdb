target remote localhost:1234

file core_symbol.img

# b bootpack.c:114
# b systask.c:12

# b msg_receive
# b msg_send
# b sys_sendrec
# b task_sys
# b u_funf

b UkiMain
# b bootpack.c:76
# b int.c:89
# b protect.c:114
b exception_handler
tui en
c
