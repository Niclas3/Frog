#include <frog/bug.h>
#include <frog/printk.h>
#include <frog/panic.h>
/**
 *	panic - halt the system
 *	@fmt: The text string to print
 *
 *	Display a message, then perform cleanups.
 *
 *	This function never returns.
 */
void panic(const char *fmt, ...)
{
        printk(fmt);
        BUG();
}
