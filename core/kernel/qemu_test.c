#include <asm/io.h>
#include <frog/printk.h>
#include <frog/types.h>
#include <kernel/qemu_test.h>

#define QEMU_DEBUG_EXIT_PORT 0xf4
#define QEMU_EXIT_PASS 0
#define QEMU_EXIT_FAIL 1

static uint_32 test_failures;
static int test_started;

static void qemu_debug_exit(uint_8 status)
{
#ifdef CONFIG_QEMU_TEST
        outb(QEMU_DEBUG_EXIT_PORT, status);
#endif
        for (;;)
                __asm__ volatile("cli; hlt");
}

void frog_test_begin(const char *profile)
{
        test_failures = 0;
        test_started = 1;
        printk("FROGTEST v=1 BEGIN profile=%s\n", profile);
}

void frog_test_case(const char *name, int passed)
{
        if (passed)
                return;
        test_failures++;
        printk("FROGTEST CASE %s FAIL\n", name);
}

void frog_test_milestone(const char *name, int passed)
{
        if (passed)
                return;
        test_failures++;
        printk("FROGTEST MILESTONE %s FAIL\n", name);
}

void frog_test_sync(const char *name)
{
        printk("FROGTEST SYNC %s\n", name);
}

void frog_test_finish(void)
{
        printk("FROGTEST END %s\n", test_failures ? "FAIL" : "PASS");
        qemu_debug_exit(test_failures ? QEMU_EXIT_FAIL : QEMU_EXIT_PASS);
}

void frog_test_abort(const char *reason)
{
        if (!test_started)
                frog_test_begin("unknown");
        test_failures++;
        printk("FROGTEST ABORT reason=%s\n", reason);
        frog_test_finish();
}
