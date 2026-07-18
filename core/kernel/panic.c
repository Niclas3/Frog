#include <asm/irqflags.h>
#include <frog/bug.h>
#include <frog/printk.h>
#include <kernel/panic.h>
#include <kernel/qemu_test.h>

/**
 *	panic - halt the system
 *	@fmt: The text string to print
 *
 *	Display a message, then perform cleanups.
 *
 *	This function never returns.
 */
void panic(const char *file, int line, const char *func, const char *msg)
{
        printk("[PANIC] %s:%d %s(): %s\n", file, line, func, msg);
#ifdef CONFIG_QEMU_TEST
        frog_test_abort("PANIC");
#endif
        __asm__ volatile("ud2");  // or __builtin_trap()

        while (1)
                arch_safe_halt();
}

void bug(const char *file, int line, const char *func)
{
        printk("[ASSERT_FAILED] %s:%d %s()\n", file, line, func);
#ifdef CONFIG_QEMU_TEST
        frog_test_abort("ASSERT_FAILED");
#endif
        __asm__ volatile("ud2");

        while (1)
                arch_safe_halt();
}
