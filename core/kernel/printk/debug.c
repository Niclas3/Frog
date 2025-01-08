#include <debug.h>
#include <stdio.h>
#include <frog/irqflags.h>
#include <frog/printk.h>

void panic_print(char *filename,
                 int line,
                 const char *func,
                 const char *condition)
{
        local_irq_disable();

        printk("filename: %s\n", filename);

        printk("line: %d\n", line);

        printk("func: %s\n", func);

        printk("case: %s\n", condition);
        while (1) {
                safe_halt();
        };
}
