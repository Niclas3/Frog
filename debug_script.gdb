target remote localhost:1234

file core_symbol.img

# b bootpack.c:100
# b bootpack.c:114
# b systask.c:12

b msg_receive
b msg_send
b sys_sendrec
# b task_sys
# b u_funf

tui en
c
