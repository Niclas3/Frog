#include <asm/irqflags.h>
#include <frog/bug.h>
#include <frog/printk.h>
#include <kernel/panic.h>

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
        __asm__ volatile("ud2");  // or __builtin_trap()

        while (1)
                arch_safe_halt();
}
