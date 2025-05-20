// start_kernel must at top of file
#include <frog/compiler.h>
#include <frog/irqflags.h>
#include <frog/syscall-init.h>

#include <frog/memory.h>
#include <frog/printk.h>
#include <frog/threads.h>
#include <kernel/bus.h>
#include <kernel/cpu.h>
#include <kernel/device.h>
#include <kernel/chardev.h>

/* #include <frog/block.h> */

#include <kernel/debug.h>

extern void init(void);
extern void cpu_idle(void);
extern void process_execute(void *, char *);
extern void platform_init(void);


extern void vfs_init(void);
extern int root_fs_init(void);
extern int dev_fs_init(void);
// test code
extern uint_32 ps2_mouse_driver_init(void);
extern uint_32 ps2_kbd_driver_init(void);

// end test




static void do_basic_setup(void)
{
        /* blk_init(); */

        // module init
        /* driver_init(); */

        // old version
        /* clock_init();// drivers */
        /* ide_init(); */
        /* fs_init(); */
        /* ps2hid_init(); */
        /* packagefs_init(); #<{(| "/dev/pkg" |)}># */
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
        cpu_idle();
}

static void isa_device_init(struct bus_type *isa_bus)
{
        // 1. init mouse device
        static struct device ps2_mouse_dev = {
            .name = "ps2-mouse",
            .io_base = 0x60,
            .irq_nr = 12,
        };
        ps2_mouse_dev.bus = isa_bus;

        // 2. init keyboard device
        static struct device ps2_kbd_dev = {
                .name = "ps2-kbd",
                .io_base = 0x60,
                .irq_nr = 1,
        };
        ps2_kbd_dev.bus = isa_bus;

        // 3. init disk device

        // 4. init rtc device (?)

        register_device(&ps2_mouse_dev);
        register_device(&ps2_kbd_dev);
}

static inline void setup_local_cpus(void)
{
        this_cpu()->current_thread = running_thread();
}


__visible void __noreturn start_kernel(void)
{
        printk_with_cls("");
        setup_local_cpus();
        platform_init();
        mem_init();
        thread_init();

        chrdev_init();
        /* blkdev_init(); */
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
        /* moudule_init(); */

        ps2_kbd_driver_init();
        ps2_mouse_driver_init();
        /****************************************/

        syscall_init();

        do_basic_setup();

        rest_init();

        /* for (;;) { */
        /*         safe_halt(); */
        /* } */
}
