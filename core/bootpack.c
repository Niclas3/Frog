#include <asm/bootpack.h>

#include <sys/descriptor.h>

#include <gua/2d_graphics.h>
#include <sys/graphic.h>

#include <frog/piti8253.h>
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

#include <device/lfbvideo.h>
#include <fs/packagefs.h>

#include <device/cmos.h>
#include <kernel_print.h>
#include <math.h>

// GUI
/* #include <gui/fsk_mouse.h> */
#include <gua/poudland-server.h>
#include <gua/poudland.h>
#include <kernel/video.h>

// test
#include <device/ide.h>
#include <fs/select.h>
#include <hid/mouse.h>
#include <ioqueue.h>
#include <list.h>
#include <panic.h>
#include <stdio.h>
#include <sys/exec.h>
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
#include <packetx.h>
#include <print.h>
#include <string.h>
#include <sys/fstask.h>
#include <sys/spinlock.h>
#include <sys/systask.h>

#include <sys/time.h>

// test things
typedef struct B_info {
    char cyls;
    char leds;
    char vmode;
    char reserve;
    short scrnx, scrny;
    unsigned char *vram;
} BOOTINFO;

typedef enum {
    BOOT_VBE_MODE,
    BOOT_VGA_MODE,
    BOOT_CGA_MODE,
    BOOT_UNKNOW
} BOOT_GFX_MODE_t;

BOOT_GFX_MODE_t g_boot_gfx_mode;
BOOT_GFX_MODE_t boot_graphics_mode(void);

void load_file_from_disk0(char *app_path,
                          uint_32 file_sz,
                          struct disk *disk,
                          uint_32 disk_offset);

void func(int a);
void funcb(int a);
void funcc(int a);
void u_fund(int a);
void u_funf(int a);
void u_fune(int a);
void u_fung(int a);


void gfx_test_print_fn(gfx_context_t *ctx,
                       uint_32 text_x,
                       uint_32 text_y,
                       uint_32 print_type);

// first user progress
void init(void);
/* struct lock main_lock; */

// test valuable
extern struct list_head process_all_list;
extern struct list_head partition_list;  // partition list
extern struct ide_channel channels[2];   // 2 different channels
extern struct partition mounted_part;    // the partition what we want to mount.
extern struct file g_file_table[];
extern struct dir root_dir;  // root directory

spinlock_t spin_lock = SPIN_LOCK_UNLOCKED;
int_32 stdin_ = 0;
int_32 stdout_ = 1;

gfx_context_t *g_ctx;

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU;  // size 4096 address 0x90000

    clock_init();

    init_idt_gdt_tss();

    mem_init();  // mem_init must early that thread_init beause thread_init need
                 // alloc memory use memory
    thread_init();
    syscall_init();

    // Set 8295A and PIT_8253 and set IF=1
    init_8259A();
    _io_sti();

    init_PIT8253();

    /* console_init(); */
    ide_init();
    fs_init();
    ps2hid_init();

    packagefs_init(); /* "/dev/pkg" */

    /************************load test programe*******************************/
    char *app_path = "/cor";
    char *argv[2] = {"a", "b"};
    uint_32 file_sz = 91 * 1024;
    char *ls_buf = sys_malloc(file_sz);
    uint_32 sectors = DIV_ROUND_UP(file_sz, 512);
    struct disk *disk0 = &channels[0].devices[0];
    struct disk *disk1 = &channels[0].devices[1];
    ide_read(disk0, 3000, ls_buf, sectors);
    int_32 fd = open(app_path, O_RDWR);
    if (fd == -1) {
        fd = open(app_path, O_CREAT | O_RDWR);
    }
    sys_write(fd, ls_buf, file_sz);
    sys_close(fd);
    sys_free(ls_buf);
    /*****************************************************************/

    /************************load test image file*****************************/
    char *image_path = "/b.bmp";
    uint_32 img_sz = 9.3 * 1024;
    uint_32 image_offset = 6144;
    load_file_from_disk0(image_path, img_sz, disk0, image_offset);
    /*****************************************************************/
    /************************load test jpeg image file******************/
    /* char *jpeg_path = "/b.jpg"; */
    /* uint_32 jpeg_sz = 2 * 1024; */
    /* uint_32 jpeg_offset = 6144; */ 
    /* load_file_from_disk0(jpeg_path, jpeg_sz, disk0, jpeg_offset); */

    /* TCB_t *freader = thread_start("aaaaaaaaaaaaaaa", 10, func, 4); */

    // init gfx memory at qemu
    // alloc 2d graphics memory
    /* gfx_context_t *g_ctx; */
    g_boot_gfx_mode = boot_graphics_mode();

    /* gfx_context_t *g_ctx; */
    if (g_boot_gfx_mode == BOOT_VBE_MODE) {
        /* twoD_graphics_init(); */
        lfb_init("qemu");

        g_ctx = init_gfx_fullscreen_double_buffer();
        if (g_ctx == NULL) {
            PANIC("video context error");
        }
        console_init(g_ctx);

        uint_32 screen_width = g_ctx->width;
        uint_32 screen_height = g_ctx->height;
        uint_32 fontsize = 8;
        struct timeval t1 = {0};

        gettimeofday(&t1, NULL);
        char path[1024] = {0};
        sys_getcwd(path, 1024);

        /* printf("-<zm@k:%s>-", path); */

        process_execute(u_fune, "app-com");  //Real app 

        /* process_execute(u_fund, "server");  // pid 5 */
        /* process_execute(u_funf, "s2_t");  // pid 4 */

        // Draw 2 Ract
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
        /* flip(g_ctx); */

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
        /* draw_backgrond(0xc00a0000, 320, 200); */
        /* draw_info((uint_8 *) 0xc00a0000, 320, COL8_848484, 20, 0, "test"); */
        /*  */
        /* char *mcursor = sys_malloc(256); */
        /* draw_cursor8(mcursor, COL8_848484); */
        /* putblock8_8((char *) 0xc00a0000, vxsize, 16, 16, mx, my, mcursor,
         * 16); */
        /* sys_free(mcursor); */

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

void func(int a) {}

void funcb(int a)
{
    while (1)
        ;
}

void funcc(int a)
{
    while (1)
        ;
}

//
//------------------------------------------------------------------------------
// process function
//------------------------------------------------------------------------------

// server
void u_fund(int a)
{
#define MAX_PACKET_SIZE 1024
    int_32 sfd = open("/dev/pkg/server", O_CREAT);
    int pid = fork();
    if (pid) {
        typedef struct pex_header {
            uint_32 *target;
            uint_8 data[];
        } pex_header_t;

        typedef struct {
            uint_32 size;
            void *source;
            char data[];
        } package_t;

        while (1) {
            uint_32 size = ioctl(sfd, IO_PACKAGEFS_QUEUE, NULL);
            if (size > 0) {
                package_t *pack = malloc(sizeof(pex_header_t) + 5);
                read(sfd, pack, MAX_PACKET_SIZE);
                printf("Client say: %s\n", pack->data);

                char *sbuf = malloc(48);
                sprintf(sbuf, "hello client this is server %d\n", getpid());
                pex_header_t *head =
                    malloc(sizeof(pex_header_t) + strlen(sbuf));
                head->target = NULL;
                strcpy(head->data, sbuf);
                write(sfd, head,
                      sizeof(pex_header_t) + strlen(sbuf));  // reply to client
            }
        }
    } else {
        int_32 cfd = open("/dev/pkg/server", O_RDONLY);
        bool has_write = 0;
        while (!has_write) {
            char *buf = malloc(48);
            sprintf(buf, "Hi! server I'am client pid is %d", getpid());
            write(cfd, buf, strlen(buf));  // write to server
            has_write = 1;
        }
        while (1) {
            uint_32 size = ioctl(cfd, IO_PACKAGEFS_QUEUE, NULL);
            if (size > 0) {
                char *pkg = malloc(MAX_PACKET_SIZE);
                read(cfd, pkg, MAX_PACKET_SIZE);
                printf("Server say: %s.\n", pkg);
            }
        }
    }
}

// test2
void u_funf(int a)
{
    int sfd = pkx_bind("server");
    if (sfd == -1) {
        exit(0);
    }
    int_32 pid = fork();
    if (pid) {
        while (1) {
            if (pkx_query(sfd)) {
                packetx_t *packet = malloc(PACKET_SIZE);
                uint_32 size = pkx_listen(sfd, packet);
                printf("XClient say: %s\n", packet->data);

                char *sbuf = malloc(48);
                sprintf(sbuf, "hello XClient this is server %d", getpid());
                void *ptr = packet->source;

                pkx_send(sfd, ptr, strlen(sbuf), sbuf);
            }
        }

    } else {
        int cfd = pkx_connect("server");

        bool has_write = 0;
        while (!has_write) {
            char *buf = malloc(48);
            sprintf(buf, "Hi! Xserver I'am client pid is %d", getpid());
            pkx_reply(cfd, strlen(buf), buf);
            has_write = 1;
        }
        while (1) {
            if (pkx_query(cfd)) {
                char *pkg = malloc(MAX_PACKET_SIZE);
                pkx_recv(cfd, pkg);
                printf("XServer say:%s.\n", pkg);
            }
        }
    }
}

// client2
void u_fung(int a)
{
    int_32 cfd = open("/dev/pkg/server", O_RDONLY);
    while (1)
        ;
}

// proc A
// Real app
void u_fune(int a)
{
    char *argv[2] = {"a", "b"};
    /* if (!fork()) { */
    execv("/cor", argv);
    /* }  */
    /* else { */
    /*     int_32 last_words; */
    /*     pid_t child_pid = wait(&last_words); */
    /*     printf("child %d is dead", child_pid); */
    /*     printf("he saied %d", last_words); */
    /* } */
}


void load_file_from_disk0(char *app_path,
                          uint_32 file_sz,
                          struct disk *disk,
                          uint_32 disk_offset)
{
    /* char *app_path = "/cor"; */
    /* uint_32 file_sz = 75 * 1024; */
    char *buf = sys_malloc(file_sz);
    uint_32 sectors = DIV_ROUND_UP(file_sz, 512);
    /* struct disk *disk0 = &channels[0].devices[0]; */
    /* struct disk *disk1 = &channels[0].devices[1]; */
    /* ide_read(disk, 384, buf, sectors); */
    ide_read(disk, disk_offset, buf, sectors);
    int_32 fd = open(app_path, O_RDWR);
    if (fd == -1) {
        fd = open(app_path, O_CREAT | O_RDWR);
    }
    sys_write(fd, buf, file_sz);
    sys_close(fd);
    sys_free(buf);
}

BOOT_GFX_MODE_t boot_graphics_mode(void)
{
    vbe_mode_info_t *p_gfx_mode = *((vbe_mode_info_t **) VBE_MODE_INFO_POINTER);
    vbe_info_t *vbe_info = *((struct vbe_info_structure **) VBE_INFO_POINTER);
    if (p_gfx_mode == 0 && vbe_info == 0) {
        return BOOT_VGA_MODE;
    } else if ((uint_32) p_gfx_mode == 1 && (uint_32) vbe_info == 1) {
        return BOOT_CGA_MODE;
    } else if ((p_gfx_mode && (uint_32) p_gfx_mode != 1) ||
               (vbe_info && (uint_32) vbe_info != 1)) {
        return BOOT_VBE_MODE;
    } else {
        return BOOT_UNKNOW;
    }
}

void init(void)
{
    uint_32 ret_pid = fork();
    if (ret_pid) {
        uint_32 ppid = getpid();
        /* printf("init pid is %d\n", getpid()); */
        while (1)
            ;
    } else {
        uint_32 cpid = getpid();
        /* printf("child pid is %d, ret id is %d\n", getpid(), ret_pid); */
        while (1)
            ;
    }
}
