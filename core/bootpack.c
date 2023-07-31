#include "include/graphic.h"
#include "include/bootpack.h"
#include "include/descriptor.h"
#include "include/int.h"
#include "include/pic.h"
#include "include/ps2mouse.h"
#include "include/keyboard.h"

#include "include/protect.h"

//-------------------------------------
typedef struct B_info {
    char cyls;
    char leds;
    char vmode;
    char reserve;
    short scrnx, scrny;
    char *vram;
} BOOTINFO;

typedef struct Color {
    unsigned char color_id;
} COLOR;

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU; // size 4096 address 0x90000
                                           
    Descriptor_REG gdtr_data={0};
    save_gdtr(&gdtr_data);

    Segment_Descriptor *gdt_start= (Segment_Descriptor *)gdtr_data.address;
    create_descriptor(gdt_start,
                      0x0,
                      0xffffffff,
                      DESC_P_1|DESC_DPL_0| DESC_S_DATA|DESC_TYPE_CODEX,
                      DESC_G_4K|DESC_D_32| DESC_L_32BITS|DESC_AVL);
    Selector selector_code = create_selector(1,TI_GDT,RPL0);

    /* init_gdt(); */
    init_idt();

    init_8259A();
    _io_sti();

    init_keyboard();
    enable_mouse();

    init_palette();
    unsigned char *vram;
    int xsize, ysize;
    vram = (unsigned char *) 0xa0000;
    xsize = 320;
    ysize = 200;

    /* char *mcursor =(char*) 0x7c00;  */
    /* char *mcursor1 =(char*) 0x7c01; */

    draw_backgrond(vram, xsize, ysize);

    /* int pysize = 16; */
    /* int pxsize = 16; */
    /* int bxsize = 16; */
    /* int vxsize = xsize; */
    /* int py0 = 50; */
    /* int px0 = 50; */

    /* draw_cursor8(mcursor, COL8_848484); */
    /* draw_cursor8(mcursor1, COL8_008484); */
    /* int mx = 70; */
    /* int my = 50; */
    /* putblock8_8((char *)vram, xsize, 16, 16, mx, my, mcursor, 16); */
    /* putblock8_8((char *)vram, xsize, 16, 16, mx+40, my+20, mcursor1, 16); */


    /* char s[16] = {0}; */
    /* sprintf(s, "A"); */

    /* putfont8(vram, xsize, 8-4, 8-4, COL8_00FF00, hankaku + 'A' * 16); */
    /* putfont8(vram, xsize, 16, 8, COL8_00FF00, hankaku + 'B' * 16); */
    /* putfont8(vram, xsize, 24, 8, COL8_00FF00, hankaku + '1' * 16); */
    /* putfonts8_asc(vram, xsize, 8, 8, COL8_0000FF, "Niclas 123"); */


    for (;;) {
        _io_hlt();  // execute _io_hlt in naskfunc.s
    }
}

