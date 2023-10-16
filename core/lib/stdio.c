#include <stdio.h>
#include <stdarg.h>
#include <string.h>

uint_32 printf(const char *fmt, ...){
    va_list ap;
    char *s;
    int d;
    char c;
    char *res;
    va_start(ap, fmt);
    while(*fmt){
        if(*fmt == '%'){
            switch(*fmt++){
            case 's':
                s = va_arg(ap, char*);
                break;
            case 'd':
                d = va_arg(ap, int);
                break;
            case 'c':
                c = (char) va_arg(ap, int);
                break;
            default:
                break;
            }
        }
    }
    va_end(ap);
    return 0;
}
