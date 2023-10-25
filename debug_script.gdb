target remote localhost:1234

file core_symbol.img

b UkiMain

# b msg_receive
# b msg_send
# b sys_sendrec
# b task_sys
# b get_ticks_mm_test
# b ipc.c:106

tui en
c
