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
#include <ioqueue.h>
#include <sys/process.h>

extern CircleQueue keyboard_queue;
extern CircleQueue mouse_queue;

void func(int a);
void funcb(int a);
void u_fund(int a);
void u_funf(int a);
void keyboard_consumer(int a);
void mouse_consumer(int a);

struct lock main_lock;

int u_test_a = 0;

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU;  // size 4096 address 0x90000
                                            //


    BOOTINFO info = {
                     /* .vram = (unsigned char *) 0xa0000, */
                     .vram = (unsigned char *) 0xc00a0000,
                     .scrnx = 320,
                     .scrny = 200,
                     .cyls = 0,
                     .leds = 0,
                     .vmode = 0,
                     .reserve = 0};
    init_idt_gdt_tss();

    thread_init();
    // Set 8295A and PIT_8253 and set IF=1
    init_8259A();
    _io_sti();
    init_keyboard();
    enable_mouse();

    init_PIT8253();

    mem_init();
    draw_backgrond(info.vram, info.scrnx, info.scrny);

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

    init_ioqueue(&keyboard_queue);
    init_ioqueue(&mouse_queue);

    TCB_t *keyboard_c = thread_start("keyboard_reader",10, keyboard_consumer , 3);
    TCB_t *mouse_c = thread_start("mouse1",10, mouse_consumer , 3);
    /* TCB_t *mouse_c1 = thread_start("mouse2",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c2 = thread_start("mouse3",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c3 = thread_start("mouse4",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c4 = thread_start("mouse5",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c11 = thread_start("mouse6",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c22 = thread_start("mouse7",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c33 = thread_start("mouse8",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c44 = thread_start("mouse9",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c11111 = thread_start("mouse10",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c12 = thread_start("mouse11",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c13 = thread_start("mouse12",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c14 = thread_start("mouse13",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c134 = thread_start("mouse14",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c1314 = thread_start("mouse15",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c213 = thread_start("mouse12",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c214 = thread_start("mouse13",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c2134 = thread_start("mouse14",10, mouse_consumer , 3); */
    /* TCB_t *mouse_c21314 = thread_start("mouse15",10, mouse_consumer , 3); */
    /* TCB_t *mouse_dc2134 = thread_start("mouse14",10, mouse_consumer , 3); */
    /* TCB_t *mouse_dc21314 = thread_start("mouse15",10, mouse_consumer , 3); */
    /* TCB_t *t  = thread_start("aaaaaaaaaaaaaaa",31, func, 4); */
    /* TCB_t *t1 = thread_start("bbbbbbbbbbbbbbb",10, funcb, 3); */
    process_execute(u_fund, "u_fund");

    for (;;) {
        /* __asm__ volatile ("sti;hlt;" : : : ); */
        _io_stihlt();
    }

}

void mouse_consumer(int a){
    int line =0;
    int x = 0;
    while(1){
        struct queue_data qdata;
        int error = ioqueue_get_data(&qdata, &mouse_queue);
        lock_fetch(&main_lock);
        if(!error){
            char code = qdata.data;
            draw_hex((uint_8 *)0xc00a0000, 320, COL8_00FF00, x, 2*16, code);
            line+=16;
            x+= 20;
        }
        lock_release(&main_lock);
        _io_stihlt();
    }
}

void keyboard_consumer(int a){
    int line = 0;
    int xpos = 0;
    for (;;) {
        struct queue_data qdata = {0};
        int error = ioqueue_get_data(&qdata, &keyboard_queue);
        lock_fetch(&main_lock);
        if(!error){
            char code = qdata.data;
            if(xpos >= 300){
                line+=16;
                xpos = 0;
            }
            put_asc_char((int_8 *)0xc00a0000, 320, xpos, line, COL8_00FFFF, code);
            xpos+= 8;
        }
        lock_release(&main_lock);
        _io_stihlt();
    }
}

void func(int a){
    while(1){
        lock_fetch(&main_lock);
        /* draw_info((uint_8 *)0xc00a0000, 320, COL8_00FF00, 100, 0, "T"); */
        if(u_test_a > 0){
            draw_hex((uint_8 *)0xc00a0000, 320, COL8_00FF00, 200, 0, u_test_a);
        }
        lock_release(&main_lock);
    }
}

void funcb(int a){
    while(1){
        lock_fetch(&main_lock);
        draw_info((uint_8 *)0xc00a0000, 320, COL8_FFFFFF, 100, 0, "T");
        draw_info((uint_8 *)0xc00a0000, 320, COL8_FFFFFF, 15, 0, "H");
        lock_release(&main_lock);
    }
}

void u_fund(int a){
    while(1){
        u_test_a++ ;
    }
}
void u_funf(int a){
    while(1){
        lock_fetch(&main_lock);
        draw_info((uint_8 *)0xc00a0000, 320, COL8_FF0000, 100, 0, "U");
        lock_release(&main_lock);
    }
}
