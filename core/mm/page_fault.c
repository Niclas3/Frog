#include <frog/memory.h>
#include <frog/types.h>
#include <kernel/panic.h>
#include "./mm_helper.h"

void page_fault_handler(uintptr_t fault_addr, uint_32 err_code)
{
        if (!page_present(err_code)) {
                if (is_lazy_alloc_region(fault_addr)) {
                        // Lazy allocation
                        mm_lazy_alloc(fault_addr);
                        return;
                } else {
                        /* kill_process(); // 非法访问 */
                        PANIC("[page fault]: access a forbidden memory");
                }
        }

        if (is_write_access(err_code) && !page_writable(fault_addr)) {
                /* if (is_COW_page(fault_addr)) { */
                /*         mm_handle_cow(fault_addr); */
                /*         return; */
                /* } else { */
                /*         PANIC("[page fault]: access a forbidden memory"); */
                /* } */
                /* kill_process(); // */
        }

        PANIC("Unhandled page fault");
}
