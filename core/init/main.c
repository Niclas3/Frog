// start_kernel must at top of file
#include <frog/compiler.h>
#include <frog/irqflags.h>
#include <frog/syscall-init.h>

#include <frog/block.h>
#include <frog/fcntl.h>
#include <frog/memory.h>
#include <frog/printk.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <kernel/bus.h>
#include <kernel/chardev.h>
#include <kernel/cpu.h>
#include <kernel/device.h>
#include <kernel/vfs.h>

/* #include <frog/block.h> */

#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/qemu_test.h>

extern void init(void);
extern void cpu_idle(void);
extern void process_execute(void *, char *);
extern void platform_init(void);

// test code
extern int root_fs_init(void);
extern int dev_fs_init(void);
extern int frogfs_init(void);
extern uint_32 ps2_mouse_driver_init(void);
extern uint_32 ps2_kbd_driver_init(void);
extern uint_32 ata_ide_driver_init(void);

extern void vga_self_test(void);
extern int mm_regression_test(void);

// end test


#include <frog/linker.h>

#ifdef CONFIG_FROG_TEST_DISK
static int bare_disk_io_test(void)
{
        struct dentry *d = vfs_lookup("/dev/sdbp8");
        if (!d || !d->d_inode) {
                WARN("[bare-io]: cannot find /dev/sdbp8");
                return -1;
        }

        struct block_device *bdev = get_block_device(d->d_inode->i_dev);
        if (!bdev) {
                WARN("[bare-io]: get_block_device returned NULL");
                return -1;
        }

        uint_32 test_lba = bdev->bd_start_lba + bdev->bd_sec_cnt - 4;
        uint_8 *wbuf = kmalloc(512);
        uint_8 *rbuf = kmalloc(512);
        if (!wbuf || !rbuf) {
                WARN("[bare-io]: alloc fail");
                return -1;
        }

        for (int i = 0; i < 512; i++)
                wbuf[i] = (uint_8) (i ^ 0xA5);
        memset(rbuf, 0, 512);

        if (bio_write(bdev, test_lba, wbuf, 1) < 0) {
                WARN("[bare-io]: bio_write fail at lba=%d", test_lba);
                kfree(wbuf);
                kfree(rbuf);
                return -1;
        }

        if (bio_read(bdev, test_lba, rbuf, 1) < 0) {
                WARN("[bare-io]: bio_read fail at lba=%d", test_lba);
                kfree(wbuf);
                kfree(rbuf);
                return -1;
        }

        int mismatch_at = -1;
        for (int i = 0; i < 512; i++) {
                if (wbuf[i] != rbuf[i]) {
                        mismatch_at = i;
                        break;
                }
        }
        if (mismatch_at >= 0) {
                WARN("[bare-io]: round-trip mismatch at byte %d: w=%x r=%x",
                     mismatch_at, wbuf[mismatch_at], rbuf[mismatch_at]);
                kfree(wbuf);
                kfree(rbuf);
                return -1;
        }

        INFO("[bare-io]: bare disk round-trip passed lba=%d (partition end-4)",
             test_lba);
        kfree(wbuf);
        kfree(rbuf);
        return 0;
}

static int frogfs_basic_io_test(void)
{
        char *paths[] = {
            "/test/frogio0", "/test/frogio1", "/test/frogio2",
            "/test/frogio3", "/test/frogio4", "/test/frogio5",
            "/test/frogio6", "/test/frogio7",
        };
        char *path = NULL;
        char payload[] = "frogfs basic io";
        char read_buf[64];
        uint_32 payload_len = strlen(payload);

        struct file *file = NULL;
        for (uint_32 idx = 0; idx < sizeof(paths) / sizeof(paths[0]); idx++) {
                file = vfs_open(paths[idx], O_CREAT | O_RDWR);
                if (file) {
                        path = paths[idx];
                        break;
                }
        }

        if (!file) {
                WARN("[frogfs-test]: create/open failed");
                return -1;
        }

        int_32 written = vfs_write(file, payload, payload_len);
        if (written != (int_32) payload_len) {
                WARN("[frogfs-test]: write failed: %d/%d", written,
                     payload_len);
                vfs_close(file);
                return -1;
        }

        if (vfs_lseek(file, 0, SEEK_SET) != 0) {
                WARN("[frogfs-test]: seek failed");
                vfs_close(file);
                return -1;
        }

        memset(read_buf, 0, sizeof(read_buf));
        int_32 read_size = vfs_read(file, read_buf, payload_len);
        if (read_size != (int_32) payload_len ||
            memcmp(read_buf, payload, payload_len)) {
                WARN("[frogfs-test]: read-after-write failed: %d/%d",
                     read_size, payload_len);
                vfs_close(file);
                return -1;
        }

        vfs_close(file);

        file = vfs_open(path, O_RDWR);
        if (!file) {
                WARN("[frogfs-test]: reopen failed: %s", path);
                return -1;
        }

        memset(read_buf, 0, sizeof(read_buf));
        read_size = vfs_read(file, read_buf, payload_len);
        if (read_size != (int_32) payload_len ||
            memcmp(read_buf, payload, payload_len)) {
                WARN("[frogfs-test]: read-after-reopen failed: %d/%d",
                     read_size, payload_len);
                vfs_close(file);
                return;
        }

        vfs_close(file);
        INFO("[frogfs-test]: create/write/read/reopen passed: %s", path);
        return 0;
}

static int frogfs_unlink_test(void)
{
        char *paths[] = {
            "/test/unlink0", "/test/unlink1", "/test/unlink2",
            "/test/unlink3", "/test/unlink4", "/test/unlink5",
        };
        char *path = NULL;

        struct file *file = NULL;
        for (uint_32 idx = 0; idx < sizeof(paths) / sizeof(paths[0]); idx++) {
                file = vfs_open(paths[idx], O_CREAT | O_RDWR);
                if (file) {
                        path = paths[idx];
                        break;
                }
        }
        if (!file) {
                WARN("[frogfs-unlink]: create failed");
                return -1;
        }
        vfs_close(file);

        struct dentry *d = vfs_lookup(path);
        if (!d || !d->d_inode) {
                WARN("[frogfs-unlink]: lookup failed after create: %s", path);
                return -1;
        }

        if (vfs_unlink(d) != 0) {
                WARN("[frogfs-unlink]: unlink failed: %s", path);
                return -1;
        }

        file = vfs_open(path, O_RDWR);
        if (file) {
                WARN("[frogfs-unlink]: file still openable after unlink: %s",
                     path);
                vfs_close(file);
                return -1;
        }

        INFO("[frogfs-unlink]: create/lookup/unlink/verify passed: %s", path);
        return 0;
}
#endif

static void do_basic_setup(void)
{
        /* DEBUG("rodata_start:%x", __rodata_start); */
        /* DEBUG("__data_start:%x", __data_start); */
        /* DEBUG("__data_end  :%x", __data_end); */
        /* DEBUG("__bss_start :%x", __bss_start); */
        /* DEBUG("_end        :%x", _end); */
        /* int a = 10; */
        /* struct file * test = kmalloc(24); */
        /* char *buf = kmalloc(300000 * 512); */
        /* struct file *f = vfs_open("/dev/sdbp1", 123);  // ide */

        DEBUG("test??");
        INFO("[INFO]: test");

        /* blk_init(); */

        // module init
        /* driver_init(); */

        // old version
        /* clock_init();// drivers */
        /* ide_init(); */
        /* fs_init(); */
        /* ps2hid_init(); */
        /* packagefs_init(); #<{(| "/dev/pkg" |)}># */

        vga_self_test();
        int mm_failures = mm_regression_test();
#ifdef CONFIG_QEMU_TEST
        frog_test_case("mm.regression", mm_failures == 0);
#endif
#ifdef CONFIG_FROG_TEST_DISK
        frog_test_case("disk.raw-roundtrip", bare_disk_io_test() == 0);
        frog_test_case("frogfs.basic-io", frogfs_basic_io_test() == 0);
        frog_test_case("frogfs.unlink", frogfs_unlink_test() == 0);
#endif
}



static void rest_init(void)
{
        // init thread pid = 1
        // dive into user mode, start first process init.
        process_execute(init, "init");
        // start a kernel thread like `kthreadd`;  we don't have it yet.
        // TODO:
        // Here is a problem, The every-early kernel thread 'unknow name' thread
        // needs to be dropped.
        TCB_t *main = running_thread();
        thread_exit(main, true);
}

static void isa_device_init(struct bus_type *isa_bus)
{
        // 1. init mouse device
        struct device *ps2_mouse_dev = kmalloc(sizeof(struct device));
        if (!ps2_mouse_dev) {
                DEBUG("[isa_dev]: cannot create ps2 mouse dev");
                return;
        }
        ps2_mouse_dev->name = "ps2-mouse";
        ps2_mouse_dev->io_base = 0x60;
        ps2_mouse_dev->irq_nr = 12;
        ps2_mouse_dev->bus = isa_bus;

        // 2. init keyboard device

        struct device *ps2_kbd_dev = kmalloc(sizeof(struct device));
        if (!ps2_kbd_dev) {
                DEBUG("[isa_dev]: cannot create ps2 kbd dev");
                return;
        }
        ps2_kbd_dev->name = "ps2-kbd";
        ps2_kbd_dev->io_base = 0x60;
        ps2_kbd_dev->irq_nr = 1;
        ps2_kbd_dev->bus = isa_bus;

        // 3. init disk device
        struct device *ata_dev = kmalloc(sizeof(struct device));
        if (!ata_dev) {
                DEBUG("[isa_dev]: cannot create ata device.");
                return;
        }
        ata_dev->name = "ata-ide";
        ata_dev->irq_nr = 12;
        ata_dev->io_base = 0x1f0;  // only for primary channel
        ata_dev->bus = isa_bus;

        // 4. init rtc device (?)

        register_device(ps2_mouse_dev);
        register_device(ps2_kbd_dev);
        register_device(ata_dev);
}

static inline void setup_local_cpus(void)
{
        this_cpu()->current_thread = running_thread();
}


__visible void __noreturn start_kernel(void)
{
        printk_with_cls("[main]: ready to init kernel...\n");
#ifdef CONFIG_QEMU_TEST
#ifdef CONFIG_FROG_TEST_DISK
        frog_test_begin("disk-smoke");
#else
        frog_test_begin("boot-smoke");
#endif
#endif
        setup_local_cpus();
        platform_init();
        mem_init();

        thread_init();
        make_main_thread();

        chrdev_init();
        block_init();
        vfs_init();

        root_fs_init();
        dev_fs_init();

        struct bus_type *isa_bus = isa_bus_init();
        struct bus_type *platform_bus = platform_bus_init();
        register_bus(isa_bus);
        register_bus(platform_bus);

        isa_device_init(isa_bus);

        // module init : to init all module that register to module.
        // But now I simulate it by call xxx_xxx_init().
        /* module_init(); */

        ps2_kbd_driver_init();
        ps2_mouse_driver_init();
        ata_ide_driver_init();

        /****************************************/
#if !defined(CONFIG_QEMU_TEST) || defined(CONFIG_FROG_TEST_DISK)
        frogfs_init();
        vfs_mount("/test", "frogfs", 0, "/dev/sdbp8", NULL);
#endif

        syscall_init();

        do_basic_setup();

        rest_init();
}
