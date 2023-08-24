#include <asm/bootpack.h>

#include <sys/descriptor.h>
#include <sys/graphic.h>
#include <sys/int.h>
#include <sys/pic.h>

#include <hid/keyboard.h>
#include <hid/ps2mouse.h>

#include <debug.h>
#include <global.h>  // global variable like keybuf
#include <oslib.h>
#include <protect.h>

// test
#include <sys/threads.h>
#include <sys/memory.h>
#include <list.h>


void func(int a);

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

    init_8259A();
    _io_sti();

    init_keyboard();
    enable_mouse();

    mem_init();

    /* init_palette(); */

    /* int pysize = 16; */
    /* int pxsize = 16; */
    /* int bxsize = 16; */
    /* int vxsize = info.scrnx; */
    /* int py0 = 50; */
    /* int px0 = 50; */
    /* int mx = 70; */
    /* int my = 50; */

    _io_cli();
    draw_backgrond(info.vram, info.scrnx, info.scrny);
    _io_sti();

    /* char *mcursor = get_kernel_page(1); */
    /* draw_cursor8(mcursor, COL8_848484); */
    /* putblock8_8((char *)info.vram, info.scrnx, 16, 16, mx, my, mcursor, 16); */

    /* TCB_t *t = thread_start("aaaaaaaaaaaaaaa",1, func, 4); */

    /* uint_32 vaddress2 = (uint_32) get_kernel_page(1); */
    /* draw_hex(info.vram, info.scrnx, COL8_848400, 0, 0, vaddress2); */
    /* draw_hex(info.vram, info.scrnx, COL8_848400, 0, 0, vaddress2); */
    /* draw_hex(info.vram, info.scrnx, COL8_848400, 16 * (5 + 2), 0, vaddress2); */
    /* uint_32 vaddress1 = (uint_32) get_kernel_page(10); */
    /*  */
    /* uint_32 vaddress3 = (uint_32) get_kernel_page(2); */
    /* draw_hex(info.vram, info.scrnx, COL8_848400, 16 * (5 + 10), 0, vaddress3); */

    /* uint_32 vaddress4 = (uint_32) get_kernel_page(0); */
    /* draw_hex(info.vram, info.scrnx, COL8_848484, 60, 0,vaddress4); */

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

void func(int a){
    int colors[6] = {
        COL8_848400,
        COL8_FFFFFF,
        COL8_000084,
        COL8_008484,
        COL8_C6C6C6,
        COL8_FF00FF
    };
    draw_info(0xa0000, 320, colors[a], 0, 0,"test");
}
