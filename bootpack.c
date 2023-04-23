/* void _io_hlt(void); */
/* void _print(char* msg, int len); */
void _print(void);

void HariMain(void)
{
    _print();
    /* __asm__("fin: hlt;" */
    /*         "jmp fin;" */
    /*         "_print: mov si, ax;" */
    /*         "_begin: mov al,[si];" */
    /*         "add si,1;" */
    /*         "cmp al,0;" */
    /*         "je fin;" */
    /*         "mov ah, 0x0e;" */
    /*         "mov bx, 15;" */
    /*         "int 10h;" */
    /*         "jmp _begin;" */
    /*         ); */
    /* fin: */
    /*     _io_hlt();   // execute _io_hlt in naskfunc.s */
    /*     goto fin; */
}

/* int main(){ */
/*     HariMain(); */
/* } */
