[bits 32]

extern main
extern exit
section .text
global _start

_start:
;; there isn't env 
    push ebx     ; push argv
    push ecx     ; push argc

    call main

    ;exit process
    push eax
    call exit

