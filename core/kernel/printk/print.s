; put_char / put_str / cls_screen moved to vga.c (C implementation with scroll).
; This file keeps put_int (hex print helper) and set_cursor only.

extern put_char

section .data
put_int_buffer    dq    0

[bits 32]
section .text

global put_int
put_int:
   pushad
   mov ebp, esp
   mov eax, [ebp+4*9]
   mov edx, eax
   mov edi, 7
   mov ecx, 8
   mov ebx, put_int_buffer

.16based_4bits:
   and edx, 0x0000000F
   cmp edx, 9
   jg .is_A2F
   add edx, '0'
   jmp .store
.is_A2F:
   sub edx, 10
   add edx, 'A'

.store:
   mov [ebx+edi], dl
   dec edi
   shr eax, 4
   mov edx, eax
   loop .16based_4bits

.ready_to_print:
   inc edi
.skip_prefix_0:
   cmp edi,8
   je .full0
.go_on_skip:
   mov cl, [put_int_buffer+edi]
   inc edi
   cmp cl, '0'
   je .skip_prefix_0
   dec edi
   jmp .put_each_num

.full0:
   mov cl,'0'
.put_each_num:
   push ecx
   call put_char
   add esp, 4
   inc edi
   mov cl, [put_int_buffer+edi]
   cmp edi,8
   jl .put_each_num
   popad
   ret

global set_cursor
set_cursor:
   pushad
   mov bx, [esp+36]
   mov dx, 0x03d4
   mov al, 0x0e
   out dx, al
   mov dx, 0x03d5
   mov al, bh
   out dx, al

   mov dx, 0x03d4
   mov al, 0x0f
   out dx, al
   mov dx, 0x03d5
   mov al, bl
   out dx, al
   popad
   ret
