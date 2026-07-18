#include <asm/io.h>
#include <frog/printk.h>
#include "./print.h"

/*
 * QEMU's isa-debugcon device captures one character per outb to port 0xe9.
 * The host runner connects this port to debugcon.log with:
 *   -device isa-debugcon,iobase=0xe9,chardev=frogdebug
 * This gives early boot and panic code a log channel without a serial driver.
 */
#define DEBUGCON_PORT 0xe9

void debugcon_put_str(const char *message)
{
        while (*message)
                outb(DEBUGCON_PORT, (uint_8) *message++);
}
