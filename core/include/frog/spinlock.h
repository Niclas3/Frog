#pragma once

#include <frog/bug.h>
#include <frog/irqflags.h>
#include <frog/types.h>

/*
 * Frog currently supports one CPU. These locks serialize interrupt and task
 * interleaving on that CPU; SMP must provide real atomic locking first.
 */
typedef struct {
        volatile uint_32 locked;
        unsigned long irq_flags;
} spinlock_t;

#define SPIN_LOCK_UNLOCKED (spinlock_t) { 0, 0 }

#define spin_is_locked(lock) ((lock).locked != 0)

#define spin_init(lock)                 \
        do {                            \
                (lock).locked = 0;      \
                (lock).irq_flags = 0;   \
        } while (0)

#define spin_lock(lock)                         \
        do {                                    \
                unsigned long __spin_flags;     \
                local_irq_save(__spin_flags);   \
                BUG_ON((lock).locked);          \
                (lock).irq_flags = __spin_flags; \
                (lock).locked = 1;              \
        } while (0)

#define spin_unlock(lock)                       \
        do {                                    \
                unsigned long __spin_flags;     \
                BUG_ON(!(lock).locked);         \
                __spin_flags = (lock).irq_flags; \
                (lock).locked = 0;              \
                (lock).irq_flags = 0;           \
                local_irq_restore(__spin_flags); \
        } while (0)

#define spin_trylock(lock)                                      \
        ({                                                      \
                unsigned long __spin_flags;                     \
                int __spin_acquired = 0;                        \
                local_irq_save(__spin_flags);                   \
                if (!(lock).locked) {                           \
                        (lock).irq_flags = __spin_flags;         \
                        (lock).locked = 1;                       \
                        __spin_acquired = 1;                    \
                } else {                                        \
                        local_irq_restore(__spin_flags);         \
                }                                               \
                __spin_acquired;                                \
        })

#define spin_lock_irqsave(lock, flags)   \
        do {                             \
                local_irq_save(flags);   \
                BUG_ON((lock).locked);   \
                (lock).irq_flags = flags; \
                (lock).locked = 1;       \
        } while (0)

#define spin_unlock_irqrestore(lock, flags) \
        do {                                \
                BUG_ON(!(lock).locked);     \
                (lock).locked = 0;          \
                (lock).irq_flags = 0;       \
                local_irq_restore(flags);   \
        } while (0)

#define spin_lock_irq(lock)          \
        do {                         \
                local_irq_disable(); \
                BUG_ON((lock).locked); \
                (lock).locked = 1;   \
        } while (0)

#define spin_unlock_irq(lock)        \
        do {                         \
                BUG_ON(!(lock).locked); \
                (lock).locked = 0;   \
                local_irq_enable();  \
        } while (0)

#define spin_unlock_wait(lock) BUG_ON(spin_is_locked(lock))
