/*
 * asm/irqflags.h
 *
 * x86 interrupt flag save and restore helpers.
 */
#ifndef _X86_IRQFLAGS_H
#define _X86_IRQFLAGS_H

#include <asm/processor-flags.h>
#include <frog/compiler.h>

static __always_inline void native_irq_enable(void)
{
        __asm__ volatile("sti" ::: "memory");
}
static __always_inline void native_irq_disable(void)
{
        __asm__ volatile("cli" ::: "memory");
}
static __always_inline void native_safe_halt(void)
{
        __asm__ volatile("sti; hlt" ::: "memory");
}

static __always_inline void native_halt(void)
{
        __asm__ volatile("hlt" ::: "memory");
}


/* This 5 function is arch-specific ,which is x86-32bits
 * */
static __always_inline int arch_irqs_disabled_flags(unsigned long flag)
{
        return !(X86_EFLAGS_IF & flag);
}

static __always_inline void arch_local_irq_enable(void)
{
        native_irq_enable();
}

static __always_inline void arch_local_irq_disable(void)
{
        native_irq_disable();
}

static __always_inline unsigned long arch_local_irq_save(void)
{
        unsigned long flags;

        __asm__ volatile("pushfl; popl %0; cli"
                         : "=r"(flags)
                         :
                         : "memory");
        return flags;
}

/* Restore the saved IF state without changing the other EFLAGS bits. */
static __always_inline void arch_local_irq_restore(unsigned long flags)
{
        if (arch_irqs_disabled_flags(flags))
                arch_local_irq_disable();
        else
                arch_local_irq_enable();
}
/* use sti; hlt;
 **/
static __always_inline void arch_safe_halt(void)
{
        native_safe_halt();
}

/* use hlt; only
 **/
static __always_inline void arch_halt(void)
{
        native_halt();
}
#endif
