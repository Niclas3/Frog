#include <asm/bootpack.h>

#include <sys/int.h>
#include <sys/pic.h>
#include <sys/graphic.h>
#include <sys/descriptor.h>

#include <hid/ps2mouse.h>
#include <hid/keyboard.h>

#include <protect.h>

extern struct KEYBUF keybuf;

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU; // size 4096 address 0x90000

    /* init_gdt(); */
    init_idt();

    init_8259A();
    _io_sti();

    init_keyboard();
    enable_mouse();

    init_palette();
    BOOTINFO info = {
        .vram = (unsigned char *) 0xa0000,
        .scrnx = 320, .scrny = 200,
        .cyls  = 0, .leds  = 0, .vmode = 0, .reserve = 0
    };

    int pysize = 16;
    int pxsize = 16;
    int bxsize = 16;
    int vxsize = info.scrnx;
    int py0 = 50;
    int px0 = 50;
    int mx = 70;
    int my = 50;

    draw_backgrond(info.vram,info.scrnx, info.scrny);

    char *mcursor =(char*) 0x7c00;
    draw_cursor8(mcursor, COL8_848484);
    putblock8_8((char *)info.vram, info.scrnx, 16, 16, mx, my, mcursor, 16);

    putfonts8_asc(info.vram, info.scrnx, 8, 8, COL8_0000FF, "Niclas 123");

    for (;;) {
        _io_cli();
        if(keybuf.flag == 0){
            _io_stihlt();
        } else {
            keybuf.flag = 0;
            _io_sti();
            int i = keybuf.data;
            putfonts8_asc(info.vram, info.scrnx, 0, 16, COL8_FFFFFF, i);
        }
    }
}

