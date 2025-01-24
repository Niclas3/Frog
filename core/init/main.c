// start_kernel must at top of file
#include <frog/compiler.h>
#include <frog/irqflags.h>
#include <frog/syscall-init.h>

#include <frog/memory.h>
#include <frog/printk.h>
#include <frog/threads.h>

extern void init(void);
extern void cpu_idle(void);
extern void process_execute(void *, char *);


static void do_basic_setup(void)
{
        /* blk_init(); */
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
        //start a kernel thread like `kthreadd`;  we don't have it yet.
        cpu_idle();
}

__visible void __noreturn start_kernel(void)
{
        printk_with_cls("test cls");
        /* setup_arch(); */
        mem_init();
        thread_init();

        syscall_init();

        do_basic_setup();

        rest_init();

        /* for (;;) { */
        /*         safe_halt(); */
        /* } */
}
