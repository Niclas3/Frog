; extern void switch_to(TCB_t *cur, TCB_t* next);

global switch_to

switch_to:
    ;Now stack top is ret address
    push esi
    push edi
    push ebx
    push ebp

    mov eax, [esp+20] ; argument `cur`
    mov [eax], esp

    ;--------------------------------------------
    ; next thread
    ;--------------------------------------------
    mov eax, [esp+24]  ; argument `next`
    mov esp, [eax]

    pop ebp
    pop ebx
    pop edi
    pop esi
    ret

