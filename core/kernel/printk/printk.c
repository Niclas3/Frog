#include <frog/printk.h>
#include <frog/stdarg.h>
#include <stdio.h>
#include "./print.h"

#include <frog/irqflags.h>
#include <frog/threads.h>
#include <kernel/cpu.h>
#include <kernel/debug.h>
#define DEMSG_LEN 1024

char __ringbuf[DEMSG_LEN];  // a ring buffer for printk

int printk(const char *fmt, ...)
{
        TCB_t *cur = this_cpu()->current_thread;
        if (!cur->pgdir) {
                va_list args;
                va_start(args, fmt);
                char buf[1024] = {0};
                vsprintf(buf, fmt, args);
                va_end(args);
                put_str(buf);
                return 0;
        }
        return 0;
}

int printk_with_cls(const char *fmt, ...)
{
        cls_screen();
        va_list args;
        va_start(args, fmt);
        char buf[1024] = {0};
        vsprintf(buf, fmt, args);
        va_end(args);
        put_str(buf);
        return 0;
}


void printk_hlt(char *filename,
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
