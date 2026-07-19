// start_kernel must at top of file
#include <frog/compiler.h>
#include <frog/irqflags.h>
#include <frog/syscall-init.h>

#include <frog/block.h>
#include <frog/fcntl.h>
#include <frog/exit.h>
#include <frog/memory.h>
#include <frog/process.h>
#include <frog/printk.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <kernel/bus.h>
#include <kernel/chardev.h>
#include <kernel/cpu.h>
#include <kernel/device.h>
#include <kernel/fs_regression.h>
#include <kernel/framebuffer_smoke.h>
#include <kernel/frogfs.h>
#include <kernel/mm_test.h>
#include <kernel/vfs.h>

/* #include <frog/block.h> */

#include <kernel/debug.h>
#include <kernel/panic.h>
#include <kernel/process_regression.h>
#include <kernel/qemu_test.h>

extern void init(void);
extern void cpu_idle(void);
extern void platform_init(void);

// test code
extern int root_fs_init(void);
extern int dev_fs_init(void);
extern int frogfs_init(void);
extern uint_32 ps2_mouse_driver_init(void);
extern uint_32 ps2_kbd_driver_init(void);
extern uint_32 ata_ide_driver_init(void);

extern void vga_self_test(void);
// end test

#ifdef CONFIG_QEMU_TEST
static void irqflags_regression_test(void)
{
        unsigned long entry_flags;
        unsigned long outer_flags;
        unsigned long inner_flags;
        unsigned long disabled_probe;
        unsigned long enabled_probe;
        unsigned long restored_probe;
        int outer_was_enabled;
        int inner_was_disabled;
        int disabled_restore_worked;
        int outer_restore_worked;
        int entry_state_restored;

        local_irq_save(entry_flags);

        local_irq_enable();
        local_irq_save(outer_flags);
        outer_was_enabled = !raw_irqs_disabled_flags(outer_flags);

        local_irq_save(inner_flags);
        inner_was_disabled = raw_irqs_disabled_flags(inner_flags);

        local_irq_enable();
        local_irq_restore(inner_flags);
        local_irq_save(disabled_probe);
        disabled_restore_worked = raw_irqs_disabled_flags(disabled_probe);
        local_irq_restore(disabled_probe);

        local_irq_restore(outer_flags);
        local_irq_save(enabled_probe);
        outer_restore_worked = !raw_irqs_disabled_flags(enabled_probe);
        local_irq_restore(enabled_probe);

        local_irq_restore(entry_flags);
        local_irq_save(restored_probe);
        entry_state_restored =
            raw_irqs_disabled_flags(restored_probe) ==
            raw_irqs_disabled_flags(entry_flags);
        local_irq_restore(restored_probe);

        frog_test_case("irqflags.outer-save-enabled", outer_was_enabled);
        frog_test_case("irqflags.inner-save-disabled", inner_was_disabled);
        frog_test_case("irqflags.restore-disabled", disabled_restore_worked);
        frog_test_case("irqflags.restore-enabled", outer_restore_worked);
        frog_test_case("irqflags.restore-entry", entry_state_restored);
}
#endif


#include <frog/linker.h>

#ifdef CONFIG_FROG_TEST_DISK
static int frogfs_test_init_result;
static int frogfs_test_mount_result;
static int frogfs_test_rollback_result;
#endif

extern const uint_8 _binary_user_smoke_basic_bin_start[];
extern const uint_8 _binary_user_smoke_basic_bin_end[];
extern const uint_8 _binary_user_smoke_process_bin_start[];
extern const uint_8 _binary_user_smoke_process_bin_end[];
extern const uint_8 _binary_user_smoke_disk_prepare_bin_start[];
extern const uint_8 _binary_user_smoke_disk_prepare_bin_end[];

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
#ifdef CONFIG_FROG_TEST_PROCESS
        process_regression_run_kernel();
#endif
#ifdef CONFIG_FROG_TEST_DISK
        fs_regression_run_kernel(frogfs_test_init_result,
                                 frogfs_test_mount_result,
                                 frogfs_test_rollback_result);
#endif
}



static void rest_init(void)
{
        // dive into user mode, start first process init.
        unsigned long flags;
        uint_32 init_pid;
        const uint_8 *image_start;
        const uint_8 *image_end;

#ifdef CONFIG_FROG_TEST_PROCESS
        image_start = _binary_user_smoke_process_bin_start;
        image_end = _binary_user_smoke_process_bin_end;
#elif defined(CONFIG_FROG_TEST_DISK) && \
      defined(CONFIG_FROG_TEST_STAGE_PREPARE)
        image_start = _binary_user_smoke_disk_prepare_bin_start;
        image_end = _binary_user_smoke_disk_prepare_bin_end;
#else
        image_start = _binary_user_smoke_basic_bin_start;
        image_end = _binary_user_smoke_basic_bin_end;
#endif

        const struct user_image image = {
            .data = image_start,
            .size = (uint_32) (image_end - image_start),
            .load_addr = USER_IMAGE_VADDR,
            .entry = USER_IMAGE_VADDR,
        };

        local_irq_save(flags);
        init_pid = process_execute_image(&image, "init");
        if (init_pid == (uint_32) -1) {
                local_irq_restore(flags);
#ifdef CONFIG_QEMU_TEST
                frog_test_abort("user-image-load");
#else
                PANIC("user image load failed");
#endif
        }
        set_init_process_pid(init_pid);
        local_irq_restore(flags);
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
        frog_test_begin(fs_regression_profile());
#elif defined(CONFIG_FROG_TEST_PROCESS)
        frog_test_begin("process-smoke");
#elif defined(CONFIG_FROG_TEST_USER)
        frog_test_begin("user-smoke");
#elif defined(CONFIG_FROG_TEST_FRAMEBUFFER)
        frog_test_begin("framebuffer-smoke");
#else
        frog_test_begin("boot-smoke");
#endif
#endif
        setup_local_cpus();
        platform_init();
        mem_init();
        thread_init();
        make_main_thread();

#ifdef CONFIG_FROG_TEST_FRAMEBUFFER
        framebuffer_smoke_run();
#endif

        if (chrdev_init() < 0)
                PANIC("chrdev initialization failed");
        if (block_init() < 0)
                PANIC("block initialization failed");
        if (vfs_init() < 0)
                PANIC("vfs initialization failed");

        if (root_fs_init() < 0)
                PANIC("rootfs initialization failed");
        if (dev_fs_init() < 0)
                PANIC("devfs initialization failed");

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
#ifdef CONFIG_QEMU_TEST
        irqflags_regression_test();
#endif

        /****************************************/
#if !defined(CONFIG_QEMU_TEST) || defined(CONFIG_FROG_TEST_DISK)
        int frogfs_init_ret = frogfs_init();
        int frogfs_mount_ret = frogfs_init_ret;
        int frogfs_rollback_ret = 0;
        int frogfs_mount_flags = 0;
#ifdef CONFIG_FROG_TEST_DISK
        frogfs_mount_flags = fs_regression_mount_flags();
#endif
        if (frogfs_init_ret == 0)
                frogfs_mount_ret =
                    vfs_mount("/test", "frogfs", frogfs_mount_flags,
                              "/dev/sdbp8", NULL);
        if (frogfs_init_ret == 0 && frogfs_mount_ret < 0)
                frogfs_rollback_ret = frogfs_init_rollback();
#ifdef CONFIG_FROG_TEST_DISK
        frogfs_test_init_result = frogfs_init_ret;
        frogfs_test_mount_result = frogfs_mount_ret;
        frogfs_test_rollback_result = frogfs_rollback_ret;
#else
        if (frogfs_init_ret < 0 || frogfs_mount_ret < 0 ||
            frogfs_rollback_ret < 0)
                WARN("[frogfs]: init=%d mount=%d rollback=%d",
                     frogfs_init_ret, frogfs_mount_ret, frogfs_rollback_ret);
#endif
#endif

        syscall_init();

        do_basic_setup();

        rest_init();
}
