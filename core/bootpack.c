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

#include <bitmap.h>


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

    /* init_gdt(); */
    init_idt();

    init_8259A();
    _io_sti();

    init_keyboard();
    enable_mouse();

    /* init_palette(); */

    /* int pysize = 16; */
    /* int pxsize = 16; */
    /* int bxsize = 16; */
    /* int vxsize = info.scrnx; */
    /* int py0 = 50; */
    /* int px0 = 50; */
    /* int mx = 70; */
    /* int my = 50; */


    draw_backgrond(info.vram, info.scrnx, info.scrny);
    uint_8 bitmap[20] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                        0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    struct bitmap bmap = {.bits = bitmap, .map_bytes_length = 20};
    init_bitmap(&bmap);
    set_value_bitmap(&bmap, 0, 1);  // 50 bit set to 1
    uint_8 a = set_block_value_bitmap(&bmap, 20, 1);
    draw_hex((char *) info.vram, info.scrnx, COL8_840000, 0, 0, a);
    /* for (int i = 0; i < (bmap.map_bytes_length) * 8; i++) { */
    /*     set_value_bitmap(&bmap, i, 1);  // 50 bit set to 1 */
    /* } */

    for (int i = 0; i < (bmap.map_bytes_length) * 8-8; i++) {
        set_value_bitmap(&bmap, i, 1);  // 50 bit set to 1
    }
    a = set_block_value_bitmap(&bmap, 3, 1);
    draw_hex((char *) info.vram, info.scrnx, COL8_840084, 0, 16, a);
    a = set_block_value_bitmap(&bmap, 5, 1);
    draw_hex((char *) info.vram, info.scrnx, COL8_840084, 0, 16+16, a);
    a = set_block_value_bitmap(&bmap, 8, 1);
    draw_hex((char *) info.vram, info.scrnx, COL8_840084, 0, 16+16+16, a);
    a = set_block_value_bitmap(&bmap, 0, 1);
    draw_hex((char *) info.vram, info.scrnx, COL8_840084, 0, 16+16+16+16, a);

    /* char *mcursor =(char*) 0x7c00; */
    /* draw_cursor8(mcursor, COL8_848484); */
    /* putblock8_8((char *)info.vram, info.scrnx, 16, 16, mx, my, mcursor, 16);
     */

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
