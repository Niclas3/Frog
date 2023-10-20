#include <asm/bootpack.h>

#include <sys/descriptor.h>
#include <sys/graphic.h>
#include <sys/int.h>
#include <sys/pic.h>
#include <sys/syscall-init.h>

#include <hid/keyboard.h>
#include <hid/ps2mouse.h>

#include <debug.h>
#include <global.h>
#include <const.h>
#include <oslib.h>
#include <protect.h>

// test
#include <sys/sched.h>
#include <sys/threads.h>
#include <sys/memory.h>
#include <list.h>
#include <sys/semaphore.h>
#include <ioqueue.h>
#include <stdio.h>
#include <sys/process.h>
#include <sys/syscall.h>

extern CircleQueue keyboard_queue;
extern CircleQueue mouse_queue;

struct list_head *map_func(struct list_head*);

void func(int a);
void funcb(int a);
void u_fund(int a);
void u_funf(int a);
void keyboard_consumer(int a);
void mouse_consumer(int a);

struct lock main_lock;

int u_test_a = 0;

struct test_struct{
    struct list_head test_tag;
    struct list_head res_tag;
    uint_32 pid;
};

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU;  // size 4096 address 0x90000
                                            //
    BOOTINFO info = {
                     .vram = (unsigned char *) 0xc00a0000,
                     .scrnx = 320,
                     .scrny = 200,
                     .cyls = 0,
                     .leds = 0,
                     .vmode = 0,
                     .reserve = 0};
    init_idt_gdt_tss();

    mem_init(); // mem_init must early that thread_init beause thread_init need
                // alloc memory use memory
    thread_init();

    syscall_init();

    // Set 8295A and PIT_8253 and set IF=1
    init_8259A();
    _io_sti();
    init_keyboard();
    enable_mouse();

    init_PIT8253();

    draw_backgrond(info.vram, info.scrnx, info.scrny);

    lock_init(&main_lock);
    init_ioqueue(&keyboard_queue);
    init_ioqueue(&mouse_queue);

    /* init_palette(); */

    int pysize = 16;
    int pxsize = 16;
    int bxsize = 16;
    int vxsize = info.scrnx;
    int py0 = 50;
    int px0 = 50;
    int mx = 70;
    int my = 50;


    TCB_t *keyboard_c = thread_start("keyboard_reader",10, keyboard_consumer , 3);
    TCB_t *mouse_c = thread_start("mouse1",10, mouse_consumer , 3);
    /* TCB_t *t  = thread_start("aaaaaaaaaaaaaaa",31, func, 4); */
    /* TCB_t *t1 = thread_start("bbbbbbbbbbbbbbb",10, funcb, 3); */
    /* process_execute(u_fund, "u_fund"); */
    /* process_execute(u_funf, "u_funf"); */

    char *str = sys_malloc(32);
    sprintf(str,"test %s %d %c and 10%%", "niclas", 22, 'c');
    draw_info((uint_8 *)0xc00a0000, 320, COL8_FF00FF, 100, 0, str);
    sys_free(str);

    mtime_sleep(1000*1*60*60); // 1 hour

    struct list_head test_list;
    struct list_head res_list;

    init_list_head(&test_list);
    init_list_head(&res_list);
    for(int i=0; i<20; i++){
        struct test_struct *ptr_test_st = sys_malloc(64);
        ptr_test_st->pid = i;
        struct list_head* node = &ptr_test_st->test_tag;
        list_add_tail(node, &test_list);
    }
    uint_32 test_list_len = list_length(&test_list);
    list_map(&test_list, &res_list, map_func);

    uint_32 res_list_len = list_length(&res_list);
    list_destory(&res_list);
    res_list_len = list_length(&res_list);
    draw_hex((uint_8*) 0xc00a0000, 320, COL8_848484, 100, 20, test_list_len);
    draw_hex((uint_8*) 0xc00a0000, 320, COL8_848400, 100, 36, res_list_len);

    char *mcursor = sys_malloc(256);
    draw_cursor8(mcursor, COL8_848484);
    putblock8_8((char *)info.vram, info.scrnx, 16, 16, mx, my, mcursor, 16);
    for (;;) {
        /* __asm__ volatile ("sti;hlt;" : : : ); */
        _io_stihlt();
    }
}

struct list_head *map_func(struct list_head* cur){
    struct list_head* current = cur;
    struct test_struct *ts =  container_of(current, struct test_struct, test_tag);
    if(ts->pid / 2 == 0){
        return &ts->res_tag;
    } else {
        return NULL;
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
    uint_32 pid = getpid();
    while(1){
        lock_fetch(&main_lock);
        draw_hex((uint_8 *)0xc00a0000, 320, COL8_00FF00, 200, 0, pid);
        lock_release(&main_lock);
    }
}

void funcb(int a){
    TCB_t *cur = running_thread();
    /* while(1){ */
        lock_fetch(&main_lock);
        draw_hex((uint_8 *)0xc00a0000, 320, COL8_00FF00, 200, 36, cur->pid);
        draw_info((uint_8 *)0xc00a0000, 320, COL8_FFFFFF, 100, 0, "T");
        draw_info((uint_8 *)0xc00a0000, 320, COL8_FF00FF, 100, 0, "H");
        lock_release(&main_lock);
    /* } */
    while(1);
}

void u_fund(int a){
        /* write("XoX"); */
    uint_32 pid = getpid();
    while(1){
        /* u_test_a++ ; */
        /* lock_fetch(&main_lock); */
        draw_hex((uint_8 *)0xc00a0000, 320, COL8_00FF00, 200, 0, pid );
        /* draw_info((uint_8 *)0xc00a0000, 320, COL8_FFFFFF, 15, 0, "Q"); */
        /* lock_release(&main_lock); */
    }
}
void u_funf(int a){
    while(1){
        int a = 0;
        a++;
    }
}
