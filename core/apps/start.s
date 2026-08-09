        .section .text._start,"ax",@progbits
        .globl _start
        .type _start, @function
_start:
        pushl %ebx
        pushl %ecx
        call main
        addl $8, %esp
        movl %eax, %ebx
        movl $7, %eax
        int $0x93
1:
        pause
        jmp 1b
        .size _start, .-_start

        .section .note.GNU-stack,"",@progbits
