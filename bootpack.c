void _io_hlt(void);
/* void _print(char* msg, int len); */
/* void _print(void); */
void _write_mem8(int addr, int data);

void HariMain(void) {
  int i;
  char *p = (char *)0xa0000;
  // 0xa0000-0x0ffff
  for (i = 0x00000; i <= 0x0ffff; i++) {
    *(p + i) = i & 0x0f;
    /* _write_mem8(i, i & 0x0f); */
  }

  for (;;) {
    _io_hlt(); // execute _io_hlt in naskfunc.s
  }
}
