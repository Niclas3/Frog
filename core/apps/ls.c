#include <stdio.h>
#include <syscall.h>

int main(int argc, char **argv)
{
    int a = 1;
    int b = 20;
    if (fork()) {
        printf("a: %d\n", a);
        while(1);
    } else {
        printf("b+a: %d\n", b + a);
        while(1);
    }
    /* return 0; */
}
