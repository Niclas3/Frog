#include <frog/irqflags.h>
#include <frog/kernel.h>
#include <frog/refcount.h>
#include <kernel/assert.h>
#include <kernel/cpu.h>
#include <kernel/panic.h>

#if MAX_CPU != 1
#error "refcount_t requires a real atomic implementation when MAX_CPU > 1"
#endif

void refcount_init(refcount_t *ref, uint_32 value)
{
        ASSERT(ref != NULL);
        ref->value = value;
}

uint_32 refcount_read(const refcount_t *ref)
{
        unsigned long flags;
        uint_32 value;

        ASSERT(ref != NULL);
        local_irq_save(flags);
        value = ref->value;
        local_irq_restore(flags);
        return value;
}

bool refcount_get_live(refcount_t *ref)
{
        unsigned long flags;
        bool acquired = false;

        ASSERT(ref != NULL);
        local_irq_save(flags);
        if (ref->value != 0 && ref->value != UINT_MAX) {
                ref->value++;
                acquired = true;
        }
        local_irq_restore(flags);
        return acquired;
}

bool refcount_put(refcount_t *ref)
{
        unsigned long flags;
        bool released;

        ASSERT(ref != NULL);
        local_irq_save(flags);
        if (ref->value == 0) {
                local_irq_restore(flags);
                PANIC("refcount underflow");
        }
        ref->value--;
        released = ref->value == 0;
        local_irq_restore(flags);
        return released;
}
