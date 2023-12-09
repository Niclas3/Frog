#include <debug.h>
#include <asm/bootpack.h>
#include <sys/graphic.h>
#include <oslib.h>
#include <stdio.h>
#include <string.h>
#include <print.h>

void panic_print(char* filename,
        int line, 
        const char* func,
        const char* condition){
    __asm__ volatile("cli;");
    
    cls_screen();

    int x = 0;
    int y = 0;
    draw_info(0xc00a0000,320,COL8_FF0000,x ,y, filename);
    printf("filename: %s\n", filename);

    y+=16;
    char tmp_str[50];
    sprintf(tmp_str,"line: %d",line);
    draw_info(0xc00a0000,320,COL8_FF0000,x ,y, tmp_str);
    printf("line: %d\n", line);

    y+=16;
    memset(tmp_str,0, 50);
    sprintf(tmp_str,"function: %s", func);
    draw_info(0xc00a0000,320,COL8_FF0000,x ,y, tmp_str);
    printf("func: %s\n", func);

    y+=16;
    memset(tmp_str,0, 50);
    sprintf(tmp_str,"case: %s", condition);
    draw_info(0xc00a0000,320,COL8_FF0000,x ,y, tmp_str);
    printf("case: %s\n", condition);
    while(1){
        __asm__ volatile("hlt;");
    };
}
