#ifndef _FROG_IRQ_FLAGS_H
#define _FROG_IRQ_FLAGS_H
#include <asm/irqflags.h>
#include <frog/typecheck.h>

#define raw_local_irq_enable()          arch_local_irq_enable()
#define raw_local_irq_disable()         arch_local_irq_disable()
#define raw_safe_halt()                 arch_safe_halt()
#define raw_local_irq_restore(flag)          \
        do {                             \
                typecheck(unsigned long,flag); \
                arch_local_irq_restore(flag); \
        } while (0)
#define raw_local_irq_save(flag) \
        do{ \
                typecheck(unsigned long,flag); \
                flag = arch_local_irq_save();  \
        } while(0)

/* raw_irqs_disabled_flags(), irqs disabled flags for test a flag is disabled 
 * flag or not.
 **/
#define raw_irqs_disabled_flags(flag) do{ arch_irqs_disabled_flags(flag); }while(0)

/* This is final local_irq_* API for other part of os.
 **/
#define local_irq_enable()          do { raw_local_irq_enable(); } while(0)
#define local_irq_disable()         do { raw_local_irq_disable(); } while(0)
#define safe_halt()                 do { raw_safe_halt(); } while(0)
#define local_irq_restore(flag)     do { raw_local_irq_restore(flag); } while(0)
#define local_irq_save(flag)        do { raw_local_irq_save(flag); } while(0)

#endif
