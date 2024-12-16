#include <ostype.h>
#include <sys/int.h>

#define IF_SET 0x00000200
#define GET_EFLAGS(EFLAG_V) __asm__ volatile("pushfl; popl %0" : "=g"(EFLAG_V));

enum intr_status intr_get_status(void)
{
    uint_32 eflags = 0;
    GET_EFLAGS(eflags);
    return (eflags & IF_SET) ? INTR_ON : INTR_OFF;
}
enum intr_status intr_set_status(enum intr_status status)
{
    return status & INTR_ON ? intr_enable() : intr_disable();
}
enum intr_status intr_enable(void)
{
    enum intr_status old_status;
    if (INTR_ON == intr_get_status()) {
        old_status = INTR_ON;
        return old_status;
    } else {
        old_status = INTR_OFF;
        __asm__ volatile("sti");
        return old_status;
    }
}
enum intr_status intr_disable(void)
{
    enum intr_status old_status;
    if (INTR_OFF == intr_get_status()) {
        old_status = INTR_OFF;
        return old_status;
    } else {
        old_status = INTR_ON;
        __asm__ volatile("cli" ::: "memory");
        return old_status;
    }
}

