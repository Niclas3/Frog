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


void func(int a);
void funcb(int a);
int test(struct list_head *node, int arg);

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


    // Set 8295A and set IF=1
    init_8259A();
    _io_sti();

    init_keyboard();
    enable_mouse();

    mem_init();

    _io_cli();
    draw_backgrond(info.vram, info.scrnx, info.scrny);
    _io_sti();

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

    /* TCB_t *t = thread_start("aaaaaaaaaaaaaaa",1, func, 4); */
    /* TCB_t *t1 = thread_start("bbbbbbbbbbbbbbb",1, funcb, 4); */

    struct list_head *list1 = get_kernel_page(1);
    struct list_head *list2 = get_kernel_page(1);
    struct list_head node1 = { .next=NULL, .prev=NULL};
    struct list_head node2 = { .next=NULL, .prev=NULL};
    struct list_head node3 = { .next=NULL, .prev=NULL};

    struct list_head node4 = { .next=NULL, .prev=NULL};
    struct list_head node5 = { .next=NULL, .prev=NULL};
    struct list_head node6 = { .next=NULL, .prev=NULL};
    init_list_head(list1);
    /* __asm__ volatile ("xchgw %bx, %bx;"); */
    list_add(&node1, list1);
    list_add(&node2, list1);
    list_add(&node3, list1);

    init_list_head(list2);
    list_add_tail(&node4, list2);
    list_add_tail(&node5, list2);
    list_add_tail(&node6, list2);
    /* __asm__ volatile ("xchgw %bx, %bx;"); */
    /* list_del_init(&node1); */
    /* list_del_init(&node2); */
    /* list_del_init(&node3); */
    /* list_del(list1); */
    /* list_append(list2, list1); */
    /* list_append_tail(list2, list1); */
    struct list_head* pick_node = map_list(list1, test, 2);
    __asm__ volatile ("xchgw %bx, %bx;");

    int len_list1 = list_length(list1);
    draw_hex(info.vram, info.scrnx, COL8_FFFF00, 0, 0, len_list1);
    if(list_find_element(list1, list1)){
        draw_info(info.vram, info.scrnx, COL8_FFFF00, 0, 16, "find!");
    } else {
        draw_info(info.vram, info.scrnx, COL8_FFFF00, 0, 16, "not find!");
    }


    /* uint_32 vaddress2 = (uint_32) get_kernel_page(1); */
    /* draw_hex(info.vram, info.scrnx, COL8_848400, 0, 0, vaddress2); */

    for (;;) {
        _io_cli();
        if (keybuf.flag == 0) {
            _io_stihlt();
        } else {
            keybuf.flag = 0;
            _io_sti();
            char scan_code[15];  // be careful with the length of the buffer
            int n = keybuf.data;
            int len = itoa(n, scan_code, 16);
            draw_info(info.vram, info.scrnx, COL8_FFFFFF, 0, 0, scan_code);
        }
    }
}

int test(struct list_head *node, int arg){
    return 1;
}

void func(int a){
    int colors[6] = {
        COL8_848400,
        COL8_FFFFFF,
        COL8_000084,
        COL8_008484,
        COL8_C6C6C6,
        COL8_FF00FF
    };
    for(int i = 0; i < 6; i++){
        boxfill8(0xa0000,320,colors[i], 20,20, 25, 25);
    }

    __asm__ volatile ("xchgw %bx, %bx;");
    /* while(1){ */
    /*     _io_cli(); */
    /*     if (keybuf.flag == 0) { */
    /*         _io_stihlt(); */
    /*     } else { */
    /*         keybuf.flag = 0; */
    /*         _io_sti(); */
    /*         char scan_code[15];  // be careful with the length of the buffer */
    /*         int n = keybuf.data; */
    /*         int len = itoa(n, scan_code, 16); */
    /*         draw_info(0xa0000, 320, COL8_FFFFFF, 0, 0, scan_code); */
    /*     } */
    /* }; */
}

void funcb(int a){
    int colors[6] = {
        COL8_848400,
        COL8_FFFFFF,
        COL8_000084,
        COL8_008484,
        COL8_C6C6C6,
        COL8_FF00FF
    };
    boxfill8(0xa0000,320,colors[4], 50, 50, 25, 25);

    __asm__ volatile ("xchgw %bx, %bx;");
    /* while(1){ */
    /*     _io_cli(); */
    /*     if (keybuf.flag == 0) { */
    /*         _io_stihlt(); */
    /*     } else { */
    /*         keybuf.flag = 0; */
    /*         _io_sti(); */
    /*         char scan_code[15];  // be careful with the length of the buffer */
    /*         int n = keybuf.data; */
    /*         int len = itoa(n, scan_code, 16); */
    /*         draw_info(0xa0000, 320, COL8_FFFFFF, 0, 0, scan_code); */
    /*     } */
    /* }; */
}
