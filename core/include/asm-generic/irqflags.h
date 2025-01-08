#ifndef _ASM_GENERIC_IRQFLAGS
#define _ASM_GENERIC_IRQFLAGS
/*
 * All architecture should implement this 4 fucntions
 * this file like a interface
 **/
#ifndef arch_local_irq_enable
 void arch_local_irq_enable(void);
#endif

#ifndef arch_local_irq_disable
 void arch_local_irq_disable(void);
#endif

#ifndef arch_local_irq_save
 unsigned long arch_local_irq_save(void);
#endif

#ifndef arch_local_irq_restore
 void arch_local_irq_restore(unsigned long flags);
#endif


#endif
