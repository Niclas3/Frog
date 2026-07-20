#include <frog/errno.h>
#include <frog/exit.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/threads.h>
#include <frog/types.h>
#include <frog/vm.h>
#include <kernel/panic.h>
#include <frog/printk.h>
#include "./mm_helper.h"

void page_fault_handler(uintptr_t fault_addr, uint_32 err_code,
                        uint_32 eip, uint_32 cs,
                        const struct context_registers *context)
{
        bool user_fault = (err_code & 0x4U) != 0;

        if (!user_fault) {
                if (!page_present(err_code) && is_kernel_space(fault_addr) &&
                    is_lazy_alloc_region(fault_addr)) {
                        mm_lazy_alloc(fault_addr);
                        return;
                }
                printk("[page fault] kernel addr=%x err=%x eip=%x cs=%x\n",
                       fault_addr, err_code, eip, cs);
                printk("[page fault] eax=%x ebx=%x ecx=%x edx=%x "
                       "esi=%x edi=%x ebp=%x esp=%x\n",
                       context->eax, context->ebx, context->ecx,
                       context->edx, context->esi, context->edi,
                       context->ebp, context->esp);
                PANIC("kernel page fault");
        }

        /* User fault classification and allocation use sleepable VM locks. */
        local_irq_enable();
        if (!page_present(err_code) &&
            vm_handle_user_page_fault(running_thread()->mm,
                                      (uint_32) fault_addr) == 0) {
                return;
        }

        TCB_t *current = running_thread();
        printk("[page fault] user pid=%d ppid=%d addr=%x err=%x "
               "eip=%x esp=%x\n",
               current->pid, current->parent_pid, fault_addr, err_code,
               eip, (uint_32) context->esp_ptr);
        sys_exit(-EFAULT);
        PANIC("user fault termination returned");
}
