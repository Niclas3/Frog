#include <panic.h>
#include <asm/bootpack.h>
#include <sys/graphic.h>

//TODO: remove it
void panic(char* s){
    putfonts8_asc_error("Kernel panic:", 0, 0);
    putfonts8_asc_error(s,16*7, 0);
    while(1){
        __asm__ volatile("cli;hlt;");
    };
}
