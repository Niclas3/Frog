#include <asm/bootpack.h>

#include <sys/descriptor.h>

#include <sys/2d_graphics.h>
#include <sys/graphic.h>

#include <sys/int.h>
#include <sys/pic.h>
#include <sys/syscall-init.h>

#include <device/console.h>
#include <device/ps2hid.h>

#include <const.h>
#include <debug.h>
#include <global.h>
#include <oslib.h>
#include <protect.h>

#include <kernel_print.h>
#include <math.h>

// GUI
/* #include <gui/fsk_mouse.h> */

// test
#include <device/ide.h>
#include <hid/mouse.h>
#include <ioqueue.h>
#include <list.h>
#include <panic.h>
#include <stdio.h>
#include <sys/memory.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/semaphore.h>
#include <sys/syscall.h>
#include <sys/threads.h>

#include <fs/dir.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <print.h>
#include <string.h>
#include <sys/fstask.h>
#include <sys/spinlock.h>
#include <sys/systask.h>
BOOT_GFX_MODE_t g_boot_gfx_mode;

// test things
typedef struct B_info {
    char cyls;
    char leds;
    char vmode;
    char reserve;
    short scrnx, scrny;
    unsigned char *vram;
} BOOTINFO;

extern CircleQueue keyboard_queue;

void func(int a);
void funcb(int a);
void funcc(int a);
void u_fund(int a);
void u_funf(int a);
void u_fune(int a);
void u_fung(int a);
void keyboard_consumer(int a);
void redraw_window(gfx_context_t *ctx);


void gfx_test_print_fn(gfx_context_t *ctx,
                       uint_32 text_x,
                       uint_32 text_y,
                       uint_32 print_type);

// first user progress
void init(void);
struct lock main_lock;

// test valuable
extern struct list_head process_all_list;
extern struct list_head partition_list;  // partition list
extern struct ide_channel channels[2];   // 2 different channels
extern struct partition mounted_part;    // the partition what we want to mount.
extern struct file g_file_table[];
extern struct dir root_dir;  // root directory

spin_lock_t spin_lock = {0};
int_32 stdin_ = 0;
int_32 stdout_ = 1;

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU;  // size 4096 address 0x90000
    lock_init(&main_lock);

    init_idt_gdt_tss();

    mem_init();  // mem_init must early that thread_init beause thread_init need
                 // alloc memory use memory
    thread_init();

    syscall_init();

    // Set 8295A and PIT_8253 and set IF=1
    init_8259A();
    _io_sti();

    init_PIT8253();

    console_init();
    ide_init();
    fs_init();

    ps2hid_init();


    // init gfx memory at qemu
    // alloc 2d graphics memory
    gfx_context_t *g_ctx;
    g_boot_gfx_mode = boot_graphics_mode();

    /* gfx_context_t *g_ctx; */
    if (g_boot_gfx_mode == BOOT_VBE_MODE) {
        twoD_graphics_init();

        /* gfx_context_t *g_ctx= init_gfx_fullscreen_double_buffer(); */
        g_ctx = init_gfx_fullscreen_double_buffer();
        if (g_ctx == NULL) {
            PANIC("vedio context error");
        }
        draw_pixel(g_ctx, 200, 200, FSK_LIME);

        uint_32 screen_width = g_ctx->width;
        uint_32 screen_height = g_ctx->height;
        uint_32 fontsize = 8;
        clear_screen(g_ctx, FSK_OLIVE);

        /* uint_32 status_bar_color = 0x88131313; */
        /* Point top_left = {.X = 0, .Y = 0}; */
        /* Point down_right = {.X = screen_width, .Y = 34}; */
        /* fill_rect_solid(g_ctx, top_left, down_right, status_bar_color); */
        /*  */
        /* top_left.X = 20; */
        /* top_left.Y = 20; */
        /* down_right.X = 40; */
        /* down_right.Y = 40; */
        /* fill_rect_solid(g_ctx, top_left, down_right, FSK_LIME | 0xff000000);
         */
        /* top_left.X = 30; */
        /* top_left.Y = 25; */
        /* down_right.X = 40 + 10; */
        /* down_right.Y = 40 + 15; */
        /* fill_rect_solid(g_ctx, top_left, down_right, 0x99D2B48C); */
        /* draw_pixel(g_ctx, 200, 200, 0xaaD2b48c); */

        // display mouse cursor
        /* uint_32 cursor_x = 200; */
        /* uint_32 cursor_y = 200; */
        /* create_fsk_mouse(g_ctx, cursor_x, cursor_y); */

        // test print number
        /* uint_32 test_dec_x = 0; */
        /* uint_32 test_dec_y = down_right.Y; */
        /* uint_32 test_hex_x = 500; */
        /* uint_32 test_hex_y = down_right.Y; */
        /* uint_32 pfn_type = 1;  // dec type */
        /* gfx_test_print_fn(g_ctx, test_dec_x, test_dec_y, pfn_type); */
        /* pfn_type = 2;  // hex type */
        /* gfx_test_print_fn(g_ctx, test_hex_x, test_hex_y, pfn_type); */

        // end test
        flip(g_ctx);

    } else if (g_boot_gfx_mode == BOOT_VGA_MODE) {
        // GUI code at bochs
        /* init_palette(); */
        /* BOOTINFO info = {.vram = (unsigned char *) 0xc00a0000, */
        /*                  .scrnx = 320, */
        /*                  .scrny = 200, */
        /*                  .cyls = 0, */
        /*                  .leds = 0, */
        /*                  .vmode = 0, */
        /*                  .reserve = 0}; */
        int pysize = 16;
        int pxsize = 16;
        int bxsize = 16;
        int vxsize = 320;
        int py0 = 50;
        int px0 = 50;
        int mx = 70;
        int my = 50;

        /* draw_backgrond(info.vram, info.scrnx, info.scrny); */
        draw_backgrond(0xc00a0000, 320, 200);
        draw_info((uint_8 *) 0xc00a0000, 320, COL8_848484, 20, 0, "test");

        char *mcursor = sys_malloc(256);
        draw_cursor8(mcursor, COL8_848484);
        putblock8_8((char *) 0xc00a0000, vxsize, 16, 16, mx, my, mcursor, 16);
        sys_free(mcursor);

    } else if (g_boot_gfx_mode == BOOT_CGA_MODE) {
        /* cls_screen(); */
        /* printf("error %d", b); */

        /* printf("1234567890a"); */
        /* char *str = sys_malloc(1024); */
        /* printf("test%d %s %x %c", 10, "zm", 15, 'z'); */
        /* sprintf(str, "test%d %s %x %c", 10, "zm", 15, 'z'); */
        /* sys_write(1, str, strlen(str)); */
        /* sys_free(str); */
        /* process_execute(u_fune, "A");  // pid 6 */

        /* char readbuf[1] = {0}; */
        /* while (1) { */
        /*     sys_read(stdin_, readbuf, 1); */
        /*     sys_write(stdout_, readbuf, 1); */
        /* } */

        /* struct dir *pdir = NULL; */
        /* struct dir_entry *dir_e = NULL; */
        /* pdir = sys_opendir("/"); */
        /* if (pdir) { */
        /*     #<{(| int_32 y = 0; |)}># */
        /*     while (dir_e = read_dir(pdir)) { */
        /*         char s[MAX_FILE_NAME_LEN] = {0}; */
        /*         sprintf(s, "%s\n", dir_e->filename); */
        /*         put_str(s); */
        /* draw_info((uint_8 *) 0xc00a0000, 320, COL8_00FF00, 240, y, s); */
        /*         #<{(| y+= 16; |)}># */
        /*     } */
        /* } */
    }

    /* TCB_t *keyboard_c = thread_start("k_reader", 10, keyboard_consumer, 3);
     */
    /* TCB_t *redraw = thread_start("redraw_thread", 42, redraw_window, g_ctx);
     */
    /* draw_info((uint_8 *) 0xc00a0000, 320, COL8_00FF00, 240, 100, "test"); */
    /* TCB_t *freader = thread_start("aaaaaaaaaaaaaaa", 10, func, 4); */
    /* TCB_t *fwriter = thread_start("bbbbbbbbbbbbbbb", 10, funcb, 3); */
    /* TCB_t *readt1 = thread_start("disk reader", 10, funcc, 3); */

    // System process at ring1
    /* process_execute_ring1(task_sys, "TASK_SYS");  // pid 2 */
    /* process_execute_ring1(task_fs, "TASK_FS");    // pid 3 */

    // User process test
    /* process_execute(u_funf, "C");  // pid 4 */
    /* process_execute(u_fund, "B");  // pid 5 */
    /* process_execute(u_fune, "A");  // pid 6 */
    process_execute(u_fune, "A");  // pid 6

    /* process_execute(u_fung, "D");  */

    for (;;) {
        // Temporary place flip in this place
        __asm__ volatile("sti;hlt;");
    }
}
// test code
void gfx_test_print_fn(gfx_context_t *ctx,
                       uint_32 text_x,
                       uint_32 text_y,
                       uint_32 print_type)
{
    uint_32 fontsize = 8;
    uint_32 base_x = text_x;
    uint_32 margin = 50;
    for (int i = 0; i < 0xff; i++) {
        if (print_type == 1) {
            draw_2d_gfx_hex(ctx, fontsize, text_x, text_y, FSK_DARK_TURQUOISE,
                            i);
        } else if (print_type == 2) {
            draw_2d_gfx_dec(ctx, fontsize, text_x, text_y, FSK_BISQUE, i);
        }
        text_x += margin;
        if (text_x % (margin * 10) == 0) {
            text_y += 20;
            text_x = base_x;
        }
    }
}

void keyboard_consumer(int a)
{
    int line = 0;
    int xpos = 0;
    for (;;) {
        char code = ioqueue_get_data(&keyboard_queue);
        lock_fetch(&main_lock);
        if (xpos >= 300) {
            line += 16;
            xpos = 0;
        }
        put_asc_char((int_8 *) 0xc00a0000, 320, xpos, line, COL8_00FFFF, code);
        xpos += 8;
        lock_release(&main_lock);
        __asm__ volatile("sti;hlt;");
    }
}

void func(int a)
{
    while (1) {
        char readbuf[1] = "x";
        spin_lock(spin_lock);

        sys_write(stdout_, readbuf, 1);

        spin_unlock(spin_lock);
    }
    // Read from file
    /* int_32 fd2 = sys_open("/test1.txt", O_RDONLY); */
    /* if (fd2 == -1) { */
    /*     while (1) */
    /*         ; */
    /*     #<{(| fd2 = sys_open("/test1.txt", O_CREAT); |)}># */
    /* } */
    /* #<{(| TCB_t *cur = running_thread(); |)}># */
    /* #<{(| struct file f2 = g_file_table[cur->fd_table[fd2]]; |)}># */
    /* #<{(|  |)}># */
    /* #<{(| f2.fd_pos = 512; |)}># */
    /* uint_32 buf_len = 512; */
    /* char *buf = sys_malloc(buf_len); */
    /* #<{(| file_read(&mounted_part, &f2, buf, buf_len); |)}># */
    /* #<{(| file_read(&mounted_part, &f2, buf, buf_len); |)}># */
    /* sys_read(fd2, buf, buf_len); */
    /*  */
    /* sys_free(buf); */
    /* sys_close(fd2); */
    //--------------------------------------------------------------------------
    /* uint_32 pid = getpid(); */
    /* while (1) { */
    /*     lock_fetch(&main_lock); */
    /*     draw_hex((uint_8 *) 0xc00a0000, 320, COL8_00FF00, 200, 0, pid); */
    /*     lock_release(&main_lock); */
    /* } */
}


void funcb(int a)
{
    while (1) {
        char readbuf[2] = "y";
        spin_lock(spin_lock);

        sys_write(stdout_, readbuf, 1);

        spin_unlock(spin_lock);
    }
    /* int_32 fd2 = sys_open("/test1.txt", O_RDWR); */
    /* if (fd2 == -1) { */
    /*     fd2 = sys_open("/test1.txt", O_CREAT); */
    /*     sys_close(fd2); */
    /*     fd2 = sys_open("/test1.txt", O_RDWR); */
    /* } */
    /*  */
    /* TCB_t *cur = running_thread(); */
    /* char buf[512] = {'V'}; */
    /* struct file f2 = g_file_table[cur->fd_table[fd2]]; */
    /* for (int i = 0; i < 140; i++) { */
    /*     int ret = 0; */
    /*     #<{(| sprintf(buf, "%c", "A"); |)}># */
    /*     ret = sys_write(fd2, buf, strlen(buf)); */
    /*     if (ret == 0) */
    /*         break; */
    /*     #<{(| file_write(&mounted_part, &f2, buf, strlen(buf)); |)}># */
    /* } */

    /* sys_lseek(fd2, -2, SEEK_END); */
    /* sys_lseek(fd2, -2, SEEK_END); */
    /* char *buf = sys_malloc(512); */
    /* sys_lseek(fd2, 1, SEEK_SET); */
    /* sys_read(fd2, buf, 12); */
    /* sys_write(fd2, "R", 1); */
    /* sys_lseek(fd2, -1, SEEK_END); */
    /* sys_read(fd2, buf, 12); */
    /* sys_lseek(fd2, -71680, SEEK_END); */
    /* sys_read(fd2, buf, 12); */
    /* sys_write(fd2, "Q", 1); */
    /* for (int i = 0; i < 65535; i++) { */
    /*     char buf[10] = {0}; */
    /*     sprintf(buf, "%d",i ); */
    /*     sys_write(fd2, buf, strlen(buf)); */
    /* } */
    /* sys_close(fd2); */

    /* TCB_t *cur = running_thread(); */
    /* #<{(| while(1){ |)}># */
    /* lock_fetch(&main_lock); */
    /* draw_hex((uint_8 *) 0xc00a0000, 320, COL8_00FF00, 200, 36, cur->pid); */
    /* draw_info((uint_8 *) 0xc00a0000, 320, COL8_FFFFFF, 100, 0, "T"); */
    /* draw_info((uint_8 *) 0xc00a0000, 320, COL8_FF00FF, 100, 0, "H"); */
    /* lock_release(&main_lock); */
    /* #<{(| } |)}># */
    /* while (1) */
    /*     ; */
}

void funcc(int a)
{
    int_32 fd2 = sys_open("/test1.txt", O_RDWR);
    if (fd2 == -1) {
        fd2 = sys_open("/test1.txt", O_CREAT);
    }
    TCB_t *cur = running_thread();
    struct file f2 = g_file_table[cur->fd_table[fd2]];
    char *data508 =
        "gfbiccageebdcefcjkbahgjihefejkchkdaebfiekbjibbdkihdfgbaddfeghfhhafaeke"
        "gagajejfjccgiiiccbefbefkhfjceaajaaabgkidkgdbdfcfieickjehddkkbeefghedhk"
        "fjbjdjafaaikdibidibhecgijhcikagbfgdhkabkhjifghdegdhgjfjcaaaecieecafhdk"
        "agkbjgfecigkikhigjcgeeikihagkkacdakkdiijbfccdccdchkcfiefgddbidbkckihge"
        "bjbicccecchadffbdaeaidkcdhaabdbiiahfcfgkcekcacjhjbgahfcfiahijcgiahijff"
        "ebafehjkgibhhekfcaacakeaaacabejkkjckhdehebjkcgidfidgggkhchkficbbekdefb"
        "kebjfahfabdaaajefdhjgchgcicjehgcceeekbeiaaahbffibbegihgbccdcbehdbiee"
        "agghbfkgfbcdgfkjijij";

    char *data504 =
        "gfbiccaebdcefcjkbahgjihefejkchkdaebfiekbjibbdkihdfgbaddfeghfhhafaeke"
        "gagajejfjccgiiiccbefbefkhfjceaajaaabgkidkgdbdfcfieickjehddkkbeefghedhk"
        "fjbjdjafaaikdibidibhecgijhcikagbfgdhkabkhjifghdegdhgjfjcaaaecieecafhdk"
        "agkbjgfecigkikhigjcgeeikihagkkacdakkdiijbfccdccdchkcfiefgddbidbkckihge"
        "bjbicccecchadffbdaeaidkcdhaabdbiiahfcfgkcekcacjhjbgahfcfiahijcgiahijff"
        "ebafehjkgibhhekfcaacakeaaacabejkckhdehebjkcgidfidgggkhchkficbbekdefb"
        "kebjfahfabdaaajefdhjgchgcicjehgcceeekbeiaaahbffibbegihgbccdcbehdbiee"
        "agghbfkgfbcdgfkjijij";

    uint_8 *data512 = sys_malloc(512);
    for (int i = 0; i < 140; i++) {
        if (i < 10) {
            sprintf(data512, "{00%d%s00%d}", i, data504, i);
        } else if (i >= 10 && i < 100) {
            sprintf(data512, "{0%d%s0%d}", i, data504, i);
        } else if (i >= 100) {
            sprintf(data512, "{%d%s%d}", i, data504, i);
        }
        file_write(&mounted_part, &f2, data512, strlen(data512));
        memset(data512, 0, 512);
    }

    sys_free(data512);
    sys_close(fd2);
}

void redraw_window(gfx_context_t *ctx)
{
    while (1) {
        flip(ctx);
    }
}
//
//------------------------------------------------------------------------------
// process function
//------------------------------------------------------------------------------

// proc B
void u_fund(int a)
{
    /* write("XoX"); */
    /* uint_32 pid = getpid(); */
    /* pid_t pid = get_pid_mm_test(); */

    while (1) {
        if (list_length(&process_all_list) >= 5) {
            // B->C
            message msg;
            reset_msg(&msg);
            msg.m_type = 1234;
            sendrec(SEND, 3, &msg);
        }
        /* u_test_a++ ; */
        /* lock_fetch(&main_lock); */
        /* draw_hex((uint_8 *)0xc00a0000, 320, COL8_00FF00, 200, 0, pid ); */
        /* draw_info((uint_8 *)0xc00a0000, 320, COL8_FFFFFF, 15, 0, "Q"); */
        /* lock_release(&main_lock); */
    }
}

// proc C
void u_funf(int a)
{
    // pid expecting 2
    /* pid_t pid_what = get_pid_mm_test(); */
    pid_t pid = getpid();
    pid_t pid_what = get_pid();
    int maybe100 = get_ticks();

    while (1) {
        // C->A
        if (list_length(&process_all_list) >= 5) {
            message msg;
            reset_msg(&msg);
            msg.m_type = 1234;
            sendrec(SEND, 5, &msg);
        }
        draw_hex((uint_8 *) 0xc00a0000, 320, COL8_00FF00, 100, 3 * 16,
                 maybe100);
        draw_hex((uint_8 *) 0xc00a0000, 320, COL8_00FF00, 100, 2 * 16,
                 pid_what);
        draw_hex((uint_8 *) 0xc00a0000, 320, COL8_00FF00, 100, 5 * 16, pid);
    }
}

// proc A
void u_fune(int a)
{

    char *pathname = "/test1.txt";
    int_32 fd = open(pathname, O_RDWR);
    if (fd == -1) {
        fd = open(pathname, O_CREAT);
        close(fd);
        fd = open(pathname, O_RDWR);
    }
    char *buf = "u_fune_test";
    write(fd, buf, strlen(buf));
    close(fd);
    /* while (1) { */
    /* uint_32 ret_pid = fork(); */
    /* if (ret_pid) { */
    /*     #<{(| uint_32 pid = getpid(); |)}># */
    /*     // I'am parents process */
    /*     #<{(| printf("Parents pid is %d\n", pid); |)}># */
    /* } else { */
    /*     // I'am child process */
    /*     __asm__ volatile("xchgw %bx, %bx;"); */
    /*     #<{(| uint_32 pid = getpid(); |)}># */
    /* } */
    while (1)
        ;
}

void u_fung(int a) {}


void init(void)
{
    /* uint_32 ret_pid = fork(); */
    /* if (ret_pid) { */
    /*     printf("Parents pid is %d", getpid()); */
    /* } else { */
    /*     printf("child pid is %d, ret id is %d", getpid(), ret_pid); */
    /* } */
    while (1)
        ;
}
