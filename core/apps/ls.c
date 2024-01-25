#include <stdio.h>

int main(int argc, char**argv)
{
    printf("I get argc:%d.\n", argc);
    for(int i =0; i < argc; i++){
        printf("NO[%d]:%s\n", i, argv[i]);
    }
    return 0;
}
