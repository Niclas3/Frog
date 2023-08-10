#include <panic.h>
#include <asm/bootpack.h>
#include <sys/graphic.h>

void panic(char* s){
    putfonts8_asc_error("Kernel panic:", 0, 0);
    putfonts8_asc_error(s,16*7, 0);
    while(1){
        _io_cli();
        _io_hlt();
    };
}
