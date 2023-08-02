#include <debug.h>
#include <asm/bootpack.h>
#include <sys/graphic.h>
#include <oslib.h>

void panic_print(char* filename,
        int line, 
        const char* func,
        const char* condition){
    _io_cli();
    int x = 0;
    int y = 0;
    putfonts8_asc_error(filename, x, y);

    y+=16;
    char s[1];
    itoa(line,s,10);
    putfonts8_asc_error(s, x, y);

    y+=16;
    putfonts8_asc_error(func, x, y);

    y+=16;
    putfonts8_asc_error(condition, x, y);
    while(1);
}
