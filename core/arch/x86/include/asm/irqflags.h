/*
 * asm/irqflags.h
 *
 * It provide a x86-arch irq restore and irq_save
 */
#ifndef _X86_IRQFLAGS_H
#define _X86_IRQFLAGS_H

#include <asm/processor-flags.h>
#include <frog/compiler.h>


static __always_inline void get_eflags(unsigned long flag)
{
        __asm__ volatile("pushfl; popl %0"
                         : "=rm"(flag)
                         : /*no input*/
                         : "memory");
}
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
        unsigned long flag = 0;
        get_eflags(flag);
        arch_local_irq_disable();
        return flag;
}

/*set interrupt enable status*/
static __always_inline void arch_local_irq_restore(unsigned long flags)
{
        if (!arch_irqs_disabled_flags(flags)) {
                arch_local_irq_enable();
        } 
        /* don't need call arch_local_irq_disable() on this `else branch`
         * because most of code structure like that 
         * `
         * arch_local_irq_save()
         * ...
         * arch_local_irq_restore()
         * `
         * arch_local_irq_save() will call irq_disable(), so most time
         * arch_local_irq_restore() called under irq_disable()
         * */
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
