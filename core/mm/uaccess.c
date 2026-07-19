#include <asm/page.h>

#include <frog/errno.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/types.h>
#include <frog/uaccess.h>

#include "mm_helper.h"

#define USER_VADDR_END 0xc0000000U

bool access_ok(const void *user_ptr, uint_32 size)
{
        uint_32 start = (uint_32) user_ptr;

        if (size == 0)
                return true;
        if (user_ptr == NULL || start < USER_VADDR_START)
                return false;
        if (size > USER_VADDR_END - USER_VADDR_START)
                return false;
        if (start >= USER_VADDR_END || start > USER_VADDR_END - size)
                return false;
        return true;
}

static bool user_pages_accessible(const void *user_ptr, uint_32 size,
                                  bool writable)
{
        TCB_t *current = running_thread();
        uint_32 page;
        uint_32 last_page;
        uint_32 required = PG_P_SET | PG_US_U;

        if (size == 0)
                return true;
        if (current == NULL || current->pgdir == NULL)
                return false;

        page = (uint_32) user_ptr & ~(PAGE_SIZE - 1);
        last_page = ((uint_32) user_ptr + size - 1) & ~(PAGE_SIZE - 1);
        if (writable)
                required |= PG_RW_W;

        for (;;) {
                uint_32 pde = *pde_ptr(page);
                uint_32 pte;

                if ((pde & required) != required)
                        return false;
                pte = *pte_ptr(page);
                if ((pte & required) != required)
                        return false;
                if (page == last_page)
                        return true;
                page += PAGE_SIZE;
        }
}

int_32 copy_from_user(void *kernel_dst, const void *user_src, uint_32 size)
{
        if (size == 0)
                return 0;
        if (kernel_dst == NULL || !access_ok(user_src, size) ||
            !user_pages_accessible(user_src, size, false))
                return -EFAULT;

        memcpy(kernel_dst, user_src, size);
        return 0;
}

int_32 copy_to_user(void *user_dst, const void *kernel_src, uint_32 size)
{
        if (size == 0)
                return 0;
        if (kernel_src == NULL || !access_ok(user_dst, size) ||
            !user_pages_accessible(user_dst, size, true))
                return -EFAULT;

        memcpy(user_dst, kernel_src, size);
        return 0;
}
