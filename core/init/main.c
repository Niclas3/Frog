// start_kernel must at top of file
#include <frog/compiler.h>
#include <frog/irqflags.h>
#include <frog/syscall-init.h>

#include <frog/memory.h>
#include <frog/printk.h>
#include <frog/threads.h>
#include <kernel/bus.h>
#include <kernel/cpu.h>

/* #include <frog/block.h> */

#include <kernel/debug.h>

extern void init(void);
extern void cpu_idle(void);
extern void process_execute(void *, char *);
extern void platform_init(void);


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

static inline void setup_local_cpus(void)
{
        this_cpu()->current_thread = running_thread();
}

__visible void __noreturn start_kernel(void)
{
        printk_with_cls("");
        /* setup_arch(); */
        setup_local_cpus();
        platform_init();
        mem_init();
        thread_init();

        struct bus_type *isa_bus = isa_bus_init();
        struct bus_type *platform_bus = platform_bus_init();
        register_bus(isa_bus);
        register_bus(platform_bus);


        syscall_init();

        do_basic_setup();

        rest_init();

        /* for (;;) { */
        /*         safe_halt(); */
        /* } */
}
