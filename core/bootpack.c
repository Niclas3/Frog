#include <asm/bootpack.h>

#include <sys/descriptor.h>
#include <sys/graphic.h>
#include <sys/int.h>
#include <sys/pic.h>
#include <sys/syscall-init.h>

#include <hid/keyboard.h>
#include <hid/ps2mouse.h>

#include <const.h>
#include <debug.h>
#include <global.h>
#include <oslib.h>
#include <protect.h>

// test
#include <device/ide.h>
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
#include <string.h>
#include <sys/fstask.h>
#include <sys/systask.h>

extern CircleQueue keyboard_queue;
extern CircleQueue mouse_queue;

void func(int a);
void funcb(int a);
void funcc(int a);
void u_fund(int a);
void u_funf(int a);
void u_fune(int a);
void u_fung(int a);
void keyboard_consumer(int a);
void mouse_consumer(int a);

struct lock main_lock;

// test valuable
extern struct list_head process_all_list;
extern struct list_head partition_list;  // partition list
extern struct ide_channel channels[2];   // 2 different channels
extern struct partition mounted_part;    // the partition what we want to mount.
extern struct file g_file_table[];
extern struct dir root_dir;  // root directory

// UkiMain must at top of file
void UkiMain(void)
{
    char *hankaku = (char *) FONT_HANKAKU;  // size 4096 address 0x90000

    BOOTINFO info = {.vram = (unsigned char *) 0xc00a0000,
                     .scrnx = 320,
                     .scrny = 200,
                     .cyls = 0,
                     .leds = 0,
                     .vmode = 0,
                     .reserve = 0};

    lock_init(&main_lock);
    init_ioqueue(&keyboard_queue);
    init_ioqueue(&mouse_queue);

    init_idt_gdt_tss();

    mem_init();  // mem_init must early that thread_init beause thread_init need
                 // alloc memory use memory
    thread_init();

    syscall_init();

    // Set 8295A and PIT_8253 and set IF=1
    init_8259A();
    _io_sti();
    init_keyboard();
    enable_mouse();

    init_PIT8253();


    init_palette();
    draw_backgrond(info.vram, info.scrnx, info.scrny);

    ide_init();

    fs_init();

    int pysize = 16;
    int pxsize = 16;
    int bxsize = 16;
    int vxsize = info.scrnx;
    int py0 = 50;
    int px0 = 50;
    int mx = 70;
    int my = 50;

    TCB_t *keyboard_c = thread_start("k_reader", 10, keyboard_consumer, 3);
    TCB_t *mouse_c = thread_start("mouse", 10, mouse_consumer, 3);
    /* TCB_t *freader = thread_start("aaaaaaaaaaaaaaa", 10, func, 4); */
    TCB_t *fwriter = thread_start("bbbbbbbbbbbbbbb", 10, funcb, 3);
    /* TCB_t *readt1 = thread_start("dick reader", 10, funcc, 3); */

    /* int_32 fd2 = sys_open("/test1.txt", O_RDONLY); */
    /* if (fd2 == -1) { */
    /*     PAINC("cannot find file"); */
    /* } */
    /* TCB_t *cur = running_thread(); */
    /* struct file f2 = g_file_table[cur->fd_table[fd2]]; */
    /*  */
    /* uint_32 buf_len = 7168; */
    /* char *buf = sys_malloc(buf_len); */
    /* file_read(&mounted_part, &f2, buf, buf_len); */
    /*  */
    /* sys_free(buf); */
    /* sys_close(fd2); */

    // System process at ring1
    /* process_execute_ring1(task_sys, "TASK_SYS");  // pid 2 */
    /* process_execute_ring1(task_fs, "TASK_FS");    // pid 3 */

    // User process test
    /* process_execute(u_funf, "C");  // pid 4 */
    /* process_execute(u_fund, "B");  // pid 5 */
    /* process_execute(u_fune, "A");  // pid 6 */

    /* process_execute(u_fung, "D");  */

    /* char *mcursor = sys_malloc(256); */
    /* draw_cursor8(mcursor, COL8_848484); */
    /* putblock8_8((char *) info.vram, info.scrnx, 16, 16, mx, my, mcursor, 16);
     */
    /* sys_free(mcursor); */
    for (;;) {
        __asm__ volatile("sti;hlt;");
    }
}


void mouse_consumer(int a)
{
    int line = 0;
    int x = 0;
    while (1) {
        struct queue_data qdata;
        int error = ioqueue_get_data(&qdata, &mouse_queue);
        lock_fetch(&main_lock);
        if (!error) {
            char code = qdata.data;
            draw_hex((uint_8 *) 0xc00a0000, 320, COL8_00FF00, x, 2 * 16, code);
            line += 16;
            x += 20;
        }
        lock_release(&main_lock);
        __asm__ volatile("sti;hlt;");
    }
}

void keyboard_consumer(int a)
{
    int line = 0;
    int xpos = 0;
    for (;;) {
        struct queue_data qdata = {0};
        int error = ioqueue_get_data(&qdata, &keyboard_queue);
        lock_fetch(&main_lock);
        if (!error) {
            char code = qdata.data;
            if (xpos >= 300) {
                line += 16;
                xpos = 0;
            }
            put_asc_char((int_8 *) 0xc00a0000, 320, xpos, line, COL8_00FFFF,
                         code);
            xpos += 8;
        }
        lock_release(&main_lock);
        __asm__ volatile("sti;hlt;");
    }
}

void func(int a)
{
    // Read from file
    int_32 fd2 = sys_open("/test1.txt", O_RDONLY);
    if (fd2 == -1) {
        while (1)
            ;
        /* fd2 = sys_open("/test1.txt", O_CREAT); */
    }
    /* TCB_t *cur = running_thread(); */
    /* struct file f2 = g_file_table[cur->fd_table[fd2]]; */
    /*  */
    /* f2.fd_pos = 512; */
    uint_32 buf_len = 512;
    char *buf = sys_malloc(buf_len);
    /* file_read(&mounted_part, &f2, buf, buf_len); */
    /* file_read(&mounted_part, &f2, buf, buf_len); */
    sys_read(fd2, buf, buf_len);

    sys_free(buf);
    sys_close(fd2);
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
    int_32 fd2 = sys_open("/test1.txt", O_RDWR);
    if (fd2 == -1) {
        fd2 = sys_open("/test1.txt", O_CREAT);
        sys_close(fd2);
    }
    /* fd2 = sys_open("/test1.txt", O_RDWR); */

    /* TCB_t *cur = running_thread(); */
    /* struct file f2 = g_file_table[cur->fd_table[fd2]]; */
    /* f2.fd_pos = 2048; */
    /* for (int i = 0; i < 10; i++) { */
    /*     char buf[10] = {0}; */
    /*     sprintf(buf, "%c", 67); */
    /*     file_write(&mounted_part, &f2, buf, strlen(buf)); */
    /* } */
    sys_lseek(fd2, -2, SEEK_END);
    /* sys_lseek(fd2, -2, SEEK_END); */
    char *buf = sys_malloc(512);
    /* sys_lseek(fd2, 1, SEEK_SET); */
    /* sys_read(fd2, buf, 12); */
    /* sys_write(fd2, "R", 1); */
    sys_lseek(fd2, -1, SEEK_END);
    sys_read(fd2, buf, 12);
    sys_lseek(fd2, -71680, SEEK_END);
    sys_read(fd2, buf, 12);
    /* sys_write(fd2, "Q", 1); */
    /* for (int i = 0; i < 65535; i++) { */
    /*     char buf[10] = {0}; */
    /*     sprintf(buf, "%d",i ); */
    /*     sys_write(fd2, buf, strlen(buf)); */
    /* } */
    sys_close(fd2);

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
    while (1) {
        if (list_length(&process_all_list) >= 5) {
            // A->B
            message msg;
            reset_msg(&msg);
            msg.m_type = 1234;
            sendrec(SEND, 4, &msg);
        }
    }
}

void u_fung(int a) {}
