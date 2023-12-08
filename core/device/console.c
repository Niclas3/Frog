#include <device/console.h>
#include <sys/semaphore.h>
#include <print.h>

struct lock gl_console_lock;

void console_init(void){
    lock_init(&gl_console_lock);
}

void console_put_char(uint_8 c){
    lock_fetch(&gl_console_lock);
    put_char(c);
    lock_release(&gl_console_lock);
}

void console_put_hex(int_32 num){
    lock_fetch(&gl_console_lock);
    put_int(num);
    lock_release(&gl_console_lock);
}

void console_put_str(char *str){
    lock_fetch(&gl_console_lock);
    put_str(str);
    lock_release(&gl_console_lock);
}
