#include <device/tty.h>
#include <sys/semaphore.h>
#include <print.h>

struct lock gl_tty_lock;

void tty_init(void){
    lock_init(&gl_tty_lock);
}

void tty_put_char(uint_8 c){
    lock_fetch(&gl_tty_lock);
    put_char(c);
    lock_release(&gl_tty_lock);
}

void tty_put_hex(int_32 num){
    lock_fetch(&gl_tty_lock);
    put_int(num);
    lock_release(&gl_tty_lock);
}

void tty_put_str(char *str){
    lock_fetch(&gl_tty_lock);
    put_str(str);
    lock_release(&gl_tty_lock);
}
