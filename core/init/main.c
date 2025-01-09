// start_kernel must at top of file
#include <frog/compiler.h>
#include <frog/irqflags.h>
#include <frog/syscall-init.h>

#include <frog/printk.h>

__visible void start_kernel(void)
{
        int b = 0;
        for (int i = 0; i< 10; i++){
                b += i;
        }
        /* printk("test"); */
        /* clock_init(); */
        /* mem_init();  // mem_init must early that thread_init beause
         * thread_init */
        /*              // need alloc memory use memory */
        /* thread_init(); */
        /* syscall_init(); */
        /* ide_init(); */
        /* fs_init(); */
        /* ps2hid_init(); */
        /* packagefs_init(); #<{(| "/dev/pkg" |)}># */

        for (;;) {
                safe_halt();
        }
}
