#include <debug.h>
#include <oslib.h>
#include <stdio.h>

void panic_print(char *filename,
                 int line,
                 const char *func,
                 const char *condition)
{
    __asm__ volatile("cli");

    printf("filename: %s\n", filename);

    printf("line: %d\n", line);

    printf("func: %s\n", func);

    printf("case: %s\n", condition);
    while (1) {
        __asm__ volatile("hlt");
    };
}
