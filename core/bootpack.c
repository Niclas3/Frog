#include <asm/bootpack.h>

#include <sys/descriptor.h>
#include <sys/graphic.h>
#include <sys/int.h>
#include <sys/pic.h>

#include <hid/keyboard.h>
#include <hid/ps2mouse.h>

#include <debug.h>
#include <global.h>  // global variable like keybuf
#include <const.h>
#include <oslib.h>
#include <protect.h>

// test
#include <sys/threads.h>
#include <sys/memory.h>
#include <list.h>
#include <sys/semaphore.h>


void func(int a);
void funcb(int a);
void keyboard_consumer(int a);

struct lock main_lock;

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU;  // size 4096 address 0x90000
                                            //


    BOOTINFO info = {.vram = (unsigned char *) 0xa0000,
                     .scrnx = 320,
                     .scrny = 200,
                     .cyls = 0,
                     .leds = 0,
                     .vmode = 0,
                     .reserve = 0};
    init_gdt();
    init_idt();

    thread_init();
    // Set 8295A and PIT_8253 and set IF=1
    init_8259A();
    _io_sti();
    init_keyboard();
    enable_mouse();

    mem_init();

    draw_backgrond(info.vram, info.scrnx, info.scrny);

    init_PIT8253();

    /* init_palette(); */

    /* int pysize = 16; */
    /* int pxsize = 16; */
    /* int bxsize = 16; */
    /* int vxsize = info.scrnx; */
    /* int py0 = 50; */
    /* int px0 = 50; */
    /* int mx = 70; */
    /* int my = 50; */

    /* char *mcursor = get_kernel_page(1); */
    /* draw_cursor8(mcursor, COL8_848484); */
    /* putblock8_8((char *)info.vram, info.scrnx, 16, 16, mx, my, mcursor, 16); */
    lock_init(&main_lock);

    TCB_t *t  = thread_start("aaaaaaaaaaaaaaa",10, func, 4);
    TCB_t *t1 = thread_start("bbbbbbbbbbbbbbb",10, funcb, 3);
    TCB_t *keyboard_c = thread_start("keyboard_reader",1, keyboard_consumer , 3);

    for (;;) {
        _io_stihlt();
    }

}

void keyboard_consumer(int a){
    for (;;) {
        if (keybuf.flag != 0) {
            char scan_code[15];  // be careful with the length of the buffer
            int n = keybuf.data;
            int len = itoa(n, scan_code, 16);
        lock_fetch(&main_lock);
            draw_info(0xa0000, 320, COL8_FFFFFF, 100, 0, scan_code);
        lock_release(&main_lock);
            keybuf.flag = 0;
        } else { 
            _io_stihlt();
        }
    }
}

void func(int a){
    while(1){
        lock_fetch(&main_lock);

        draw_info((uint_8 *)0xa0000, 320, COL8_00FF00, 100, 0, "T");

        lock_release(&main_lock);
    }
}

void funcb(int a){
    while(1){
        lock_fetch(&main_lock);

        draw_info((uint_8 *)0xa0000, 320, COL8_FFFFFF, 100, 0, "T");
        draw_info((uint_8 *)0xa0000, 320, COL8_FFFFFF, 15, 0, "H");

        lock_release(&main_lock);
    }
}
