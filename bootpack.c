void _io_hlt(void);

void HariMain(void)
{
    fin:
        _io_hlt();   // execute _io_hlt in naskfunc.s
        goto fin;
}
