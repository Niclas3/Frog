TI_GDT equ  0
RPL0  equ   0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

section .data
put_int_buffer    dq    0

[bits 32]
section .text

global put_str
; ---
; void put_str(char *buf){
;    char *a = buf;
;      while(*a){
;         put_char(a);
;         a++;
;   }
;}

put_str:
   push ebx
   push ecx
   xor ecx, ecx		      
   mov ebx, [esp + 12]	; buf move to ebx      
.goon:
   mov cl, [ebx]        ; move first char to cl(8 bits) == char (1 byte)
   cmp cl, 0		; test if this char is end-of-line aka \0
   jz .str_over         ; if cl == 0 then jmp to .str_over
   push ecx             ; pass ecx aka char to put_char
   call put_char        ; put_char(ecx)
   add esp, 4		; clear params / for push ecx
   inc ebx		; ebx contains buf address it mean buf++;
   jmp .goon            ; 
.str_over:
   pop ecx
   pop ebx
   ret

;------------------------   put_char   -----------------------------
; void put_char(char c){
; }
;-------------------------------------------------------------------   
global put_char
put_char:
   pushad	           ; save all register (EAX ~ EDI)
   push gs
   mov ax, SELECTOR_VIDEO  ; 
   mov gs, ax              ; set gs to SELECTOR_VIDEO

   mov dx, 0x03d4          ; read current cursor position from IO port 0x03d4
   mov al, 0x0e	           ; read high-byte 
   out dx, al              ; tell VGA i want to read number 0x0E register
   mov dx, 0x03d5          ;
   in al, dx               ; read numebr 0x0E register data from 0x03D5
   mov ah, al              ; save al to ah

   mov dx, 0x03d4          ; 
   mov al, 0x0f            ; same things but 0x0f means read low-byte
   out dx, al
   mov dx, 0x03d5
   in al, dx               ; data save in al so the ax has all data

   mov bx, ax
   mov ecx, [esp + 40]     ; pushad==32bytes push gs == 4 bytes + params 4bytes 32+4+4 == 40 bytes
                           ; get params to ecx
   cmp cl, 0xd	           ; test CR (Carriage Return) \r
   jz .is_carriage_return  ; 
   cmp cl, 0xa             ; test is it is LF (line feed) \n
   jz .is_line_feed

   cmp cl, 0x8	           ; test BS (Backspace) \b
   jz .is_backspace
   jmp .put_other

 .is_backspace:		      
   dec bx
   shl bx,1
   mov byte [gs:bx], 0x20
   inc bx
   mov byte [gs:bx], 0x07
   shr bx,1
   jmp .set_cursor

 .put_other:
   shl bx, 1               ; in VGA every character has 2 bytes the first is
                           ; ASCII character; the second is attribute (color, bright, blink)
   mov [gs:bx], cl
   inc bx
   mov byte [gs:bx],0x07
   shr bx, 1
   inc bx
   cmp bx, 2000            ; 80 col x 25 lines == 2000 char
   jl .set_cursor
 .is_line_feed:
 .is_carriage_return:
   xor dx, dx
   mov ax, bx
   mov si, 80
   div si
   sub bx, dx
 .is_carriage_return_end:
   add bx, 80
   cmp bx, 2000
 .is_line_feed_end:
   jl .set_cursor

 .roll_screen:
   cld  
   mov ecx, 960         ; ready to copy 80 col x 24 lines == 1920 bytes == 960 double bytes(64 bits)
   mov esi, 0xc00b80a0
   mov edi, 0xc00b8000
   rep movsd

   mov ebx, 3840        ; 24 lines * 80 col * 2 (bytes) last line start
   mov ecx, 80          ; cols number
 .cls
   mov word [gs:ebx], 0x0720  ; space + write
   add ebx, 2
   loop .cls 
   mov bx,1920				 

 .set_cursor:
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
 .put_char_done: 
   pop gs
   popad
   ret

global cls_screen
cls_screen:
   pushad
   mov ax, SELECTOR_VIDEO	       
   mov gs, ax

   mov ebx, 0
   mov ecx, 80*25
 .cls:
   mov word [gs:ebx], 0x0720		  
   add ebx, 2
   loop .cls 
   mov ebx, 0

 .set_cursor:				  
   mov dx, 0x03d4			  ;
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
