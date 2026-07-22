#include <asm/page.h>

#include <frog/elf.h>
#include <frog/errno.h>
#include <frog/exec.h>
#include <frog/fcntl.h>
#include <frog/irqflags.h>
#include <frog/memory.h>
#include <frog/process.h>
#include <frog/string.h>
#include <frog/threads.h>
#include <frog/uaccess.h>
#include <frog/vm.h>
#include <kernel/vfs.h>

#define EXEC_MAX_ARGC              32U
#define EXEC_STACK_HEADROOM        1024U
#define EXEC_ARG_BYTES_MAX         \
        (PAGE_SIZE - EXEC_STACK_HEADROOM - \
         (EXEC_MAX_ARGC + 1U) * sizeof(uint_32))
#define EXEC_MAX_LOAD_SEGMENTS     32U
#define EXEC_MAX_IMAGE_PAGES       4096U
#define EXEC_MAX_FILE_OFFSET       0x7fffffffU
#define EXEC_USER_IMAGE_END        VM_MMAP_START

struct exec_arguments {
        char *path;
        char *storage;
        char *values[EXEC_MAX_ARGC];
        uint_32 lengths[EXEC_MAX_ARGC];
        uint_32 argc;
        uint_32 bytes;
};

struct exec_segment {
        Elf32_Phdr phdr;
        uint_32 map_start;
        uint_32 map_end;
};

struct exec_image_plan {
        struct exec_segment segments[EXEC_MAX_LOAD_SEGMENTS];
        uint_32 segment_count;
        uint_32 page_count;
        uint_32 entry;
};

extern void intr_exit(void);

static void exec_release_arguments(struct exec_arguments *arguments)
{
        if (arguments == NULL)
                return;
        kfree(arguments->storage);
        kfree(arguments->path);
        memset(arguments, 0, sizeof(*arguments));
}

static int exec_copy_arguments(const char *user_path,
                               const char *const user_argv[],
                               struct exec_arguments *arguments)
{
        uint_32 path_length;
        int result;

        if (arguments == NULL)
                return -EINVAL;
        memset(arguments, 0, sizeof(*arguments));
        arguments->path = kmalloc(PATH_NAME_MAX + 1U);
        arguments->storage = kmalloc(EXEC_ARG_BYTES_MAX);
        if (arguments->path == NULL || arguments->storage == NULL) {
                result = -ENOMEM;
                goto fail;
        }

        result = copy_string_from_user(arguments->path, user_path,
                                       PATH_NAME_MAX + 1U, &path_length);
        if (result != 0)
                goto fail;
        if (path_length == 0) {
                result = -ENOENT;
                goto fail;
        }
        if (user_argv == NULL)
                return 0;

        for (uint_32 index = 0; index <= EXEC_MAX_ARGC; index++) {
                unsigned long long slot =
                    (unsigned long long) (uint_32) user_argv +
                    index * sizeof(uint_32);
                uint_32 user_value;

                if (slot > 0xffffffffULL) {
                        result = -EFAULT;
                        goto fail;
                }
                result = copy_from_user(&user_value,
                                        (const void *) (uint_32) slot,
                                        sizeof(user_value));
                if (result != 0)
                        goto fail;
                if (user_value == 0)
                        return 0;
                if (index == EXEC_MAX_ARGC ||
                    arguments->bytes == EXEC_ARG_BYTES_MAX) {
                        result = -E2BIG;
                        goto fail;
                }

                arguments->values[index] =
                    arguments->storage + arguments->bytes;
                result = copy_string_from_user(
                    arguments->values[index], (const char *) user_value,
                    EXEC_ARG_BYTES_MAX - arguments->bytes,
                    &arguments->lengths[index]);
                if (result == -ENAMETOOLONG)
                        result = -E2BIG;
                if (result != 0)
                        goto fail;
                arguments->bytes += arguments->lengths[index] + 1U;
                arguments->argc++;
        }

        result = -E2BIG;
fail:
        exec_release_arguments(arguments);
        return result;
}

static int exec_read_exact_at(struct file *file,
                              uint_32 offset,
                              void *buffer,
                              uint_32 length)
{
        uint_8 *output = buffer;
        uint_32 completed = 0;
        int_32 result;

        if (file == NULL || (buffer == NULL && length != 0) ||
            offset > EXEC_MAX_FILE_OFFSET ||
            length > EXEC_MAX_FILE_OFFSET - offset)
                return -ENOEXEC;
        result = vfs_lseek(file, (int_32) offset, SEEK_SET);
        if (result < 0)
                return result;
        if ((uint_32) result != offset)
                return -EIO;

        while (completed < length) {
                result = vfs_read(file, output + completed,
                                  length - completed);
                if (result < 0)
                        return result;
                if (result == 0 || (uint_32) result > length - completed)
                        return -ENOEXEC;
                completed += (uint_32) result;
        }
        return 0;
}

static bool exec_power_of_two(uint_32 value)
{
        return value != 0 && (value & (value - 1U)) == 0;
}

static bool exec_ranges_overlap(uint_32 first_start,
                                uint_32 first_end,
                                uint_32 second_start,
                                uint_32 second_end)
{
        return first_start < second_end && second_start < first_end;
}

static bool exec_page_in_earlier_segment(const struct exec_image_plan *plan,
                                         uint_32 limit,
                                         uint_32 page)
{
        for (uint_32 index = 0; index < limit; index++) {
                if (page >= plan->segments[index].map_start &&
                    page < plan->segments[index].map_end)
                        return true;
        }
        return false;
}

static bool exec_page_writable(const struct exec_image_plan *plan,
                               uint_32 page)
{
        for (uint_32 index = 0; index < plan->segment_count; index++) {
                const struct exec_segment *segment = &plan->segments[index];

                if (page >= segment->map_start && page < segment->map_end &&
                    (segment->phdr.p_flags & PF_W))
                        return true;
        }
        return false;
}

static int exec_validate_header(const Elf32_Ehdr *header, uint_32 file_size)
{
        unsigned long long table_end;

        if (header->e_ident[EI_MAG0] != ELFMAG0 ||
            header->e_ident[EI_MAG1] != ELFMAG1 ||
            header->e_ident[EI_MAG2] != ELFMAG2 ||
            header->e_ident[EI_MAG3] != ELFMAG3 ||
            header->e_ident[EI_CLASS] != ELFCLASS32 ||
            header->e_ident[EI_DATA] != ELFDATA2LSB ||
            header->e_ident[EI_VERSION] != EV_CURRENT ||
            header->e_type != ET_EXEC || header->e_machine != EM_386 ||
            header->e_version != EV_CURRENT ||
            header->e_ehsize != sizeof(*header) ||
            header->e_phentsize != sizeof(Elf32_Phdr) ||
            header->e_phnum == 0 ||
            header->e_phnum > EXEC_MAX_LOAD_SEGMENTS ||
            header->e_phoff < sizeof(*header))
                return -ENOEXEC;

        table_end = (unsigned long long) header->e_phoff +
                    (unsigned long long) header->e_phnum *
                        header->e_phentsize;
        if (table_end > file_size || table_end > EXEC_MAX_FILE_OFFSET)
                return -ENOEXEC;
        return 0;
}

static int exec_add_load_segment(struct exec_image_plan *plan,
                                 const Elf32_Phdr *phdr,
                                 uint_32 file_size,
                                 uint_32 entry,
                                 bool *entry_is_executable)
{
        unsigned long long file_end;
        unsigned long long memory_end;
        unsigned long long aligned_end;
        uint_32 map_start;
        uint_32 map_end;
        uint_32 new_pages = 0;

        /* Frog's i386 paging ABI cannot represent unreadable user pages. */
        if (phdr->p_filesz > phdr->p_memsz ||
            (phdr->p_flags & ~(PF_R | PF_W | PF_X)) != 0 ||
            !(phdr->p_flags & PF_R))
                return -ENOEXEC;
        if (phdr->p_align > 1U &&
            (!exec_power_of_two(phdr->p_align) ||
             (phdr->p_vaddr & (phdr->p_align - 1U)) !=
                 (phdr->p_offset & (phdr->p_align - 1U))))
                return -ENOEXEC;

        file_end = (unsigned long long) phdr->p_offset + phdr->p_filesz;
        if (phdr->p_offset > file_size || file_end > file_size ||
            file_end > EXEC_MAX_FILE_OFFSET)
                return -ENOEXEC;
        if (phdr->p_memsz == 0)
                return 0;

        memory_end = (unsigned long long) phdr->p_vaddr + phdr->p_memsz;
        aligned_end = (memory_end + PAGE_SIZE - 1U) &
                      ~(unsigned long long) (PAGE_SIZE - 1U);
        map_start = phdr->p_vaddr & ~(PAGE_SIZE - 1U);
        if (phdr->p_vaddr < USER_VADDR_START ||
            map_start < USER_VADDR_START ||
            memory_end > EXEC_USER_IMAGE_END ||
            aligned_end > EXEC_USER_IMAGE_END)
                return -ENOEXEC;
        map_end = (uint_32) aligned_end;
        if ((map_end - map_start) / PAGE_SIZE > EXEC_MAX_IMAGE_PAGES)
                return -E2BIG;

        for (uint_32 index = 0; index < plan->segment_count; index++) {
                const Elf32_Phdr *existing =
                    &plan->segments[index].phdr;
                uint_32 existing_end = existing->p_vaddr +
                                       existing->p_memsz;

                if (exec_ranges_overlap(phdr->p_vaddr,
                                        (uint_32) memory_end,
                                        existing->p_vaddr, existing_end))
                        return -ENOEXEC;
        }

        for (uint_32 page = map_start; page < map_end;
             page += PAGE_SIZE) {
                if (!exec_page_in_earlier_segment(
                        plan, plan->segment_count, page))
                        new_pages++;
        }
        if (new_pages > EXEC_MAX_IMAGE_PAGES - plan->page_count)
                return -E2BIG;

        struct exec_segment *segment =
            &plan->segments[plan->segment_count++];
        segment->phdr = *phdr;
        segment->map_start = map_start;
        segment->map_end = map_end;
        plan->page_count += new_pages;
        if ((phdr->p_flags & PF_X) && entry >= phdr->p_vaddr &&
            (unsigned long long) entry < memory_end)
                *entry_is_executable = true;
        return 0;
}

static int exec_build_plan(struct file *file, struct exec_image_plan *plan)
{
        Elf32_Ehdr header;
        uint_32 file_size;
        bool entry_is_executable = false;
        int result;

        if (file == NULL || file->f_inode == NULL || file->f_dentry == NULL ||
            file->f_dentry->d_type != FT_REGULAR)
                return -ENOEXEC;
        file_size = file->f_inode->i_size;
        if (file_size < sizeof(header))
                return -ENOEXEC;

        result = exec_read_exact_at(file, 0, &header, sizeof(header));
        if (result != 0)
                return result;
        result = exec_validate_header(&header, file_size);
        if (result != 0)
                return result;

        memset(plan, 0, sizeof(*plan));
        plan->entry = header.e_entry;
        for (uint_32 index = 0; index < header.e_phnum; index++) {
                Elf32_Phdr phdr;
                uint_32 offset = header.e_phoff +
                                 index * header.e_phentsize;

                result = exec_read_exact_at(file, offset, &phdr,
                                            sizeof(phdr));
                if (result != 0)
                        return result;
                if (phdr.p_type == PT_INTERP || phdr.p_type == PT_DYNAMIC ||
                    phdr.p_type == PT_TLS)
                        return -ENOEXEC;
                if (phdr.p_type != PT_LOAD)
                        continue;
                if (plan->segment_count == EXEC_MAX_LOAD_SEGMENTS)
                        return -E2BIG;
                result = exec_add_load_segment(
                    plan, &phdr, file_size, header.e_entry,
                    &entry_is_executable);
                if (result != 0)
                        return result;
        }

        if (plan->segment_count == 0 || !entry_is_executable)
                return -ENOEXEC;
        return 0;
}

static int exec_load_segments(struct file *file,
                              const struct exec_image_plan *plan,
                              struct mm_struct *mm)
{
        uint_8 *buffer;
        int result = 0;

        for (uint_32 index = 0; index < plan->segment_count; index++) {
                const struct exec_segment *segment = &plan->segments[index];

                for (uint_32 page = segment->map_start;
                     page < segment->map_end; page += PAGE_SIZE) {
                        if (exec_page_in_earlier_segment(plan, index, page))
                                continue;
                        result = vm_user_map_owned_page(mm, page);
                        if (result != 0)
                                return result;
                }
        }

        buffer = get_kernel_page(1);
        if (buffer == NULL)
                return -ENOMEM;
        for (uint_32 index = 0; index < plan->segment_count; index++) {
                const Elf32_Phdr *phdr = &plan->segments[index].phdr;
                uint_32 completed = 0;

                while (completed < phdr->p_filesz) {
                        uint_32 chunk = phdr->p_filesz - completed;

                        if (chunk > PAGE_SIZE)
                                chunk = PAGE_SIZE;
                        result = exec_read_exact_at(
                            file, phdr->p_offset + completed, buffer, chunk);
                        if (result != 0)
                                goto out;
                        result = vm_user_write_owned(
                            mm, phdr->p_vaddr + completed, buffer, chunk);
                        if (result != 0)
                                goto out;
                        completed += chunk;
                }
        }

        for (uint_32 index = 0; index < plan->segment_count; index++) {
                const struct exec_segment *segment = &plan->segments[index];

                for (uint_32 page = segment->map_start;
                     page < segment->map_end; page += PAGE_SIZE) {
                        if (exec_page_in_earlier_segment(plan, index, page))
                                continue;
                        result = vm_user_protect_owned(
                            mm, page, PAGE_SIZE,
                            exec_page_writable(plan, page));
                        if (result != 0)
                                goto out;
                }
        }

out:
        free_page(MP_KERNEL, buffer, 1);
        return result;
}

static int exec_build_stack(struct mm_struct *mm,
                            const struct exec_arguments *arguments,
                            uint_32 *stack_out,
                            uint_32 *argv_out)
{
        uint_8 *page;
        uint_32 pointers[EXEC_MAX_ARGC + 1U];
        uint_32 cursor = PAGE_SIZE;
        uint_32 pointer_bytes;
        int result;

        page = get_kernel_page(1);
        if (page == NULL)
                return -ENOMEM;
        memset(page, 0, PAGE_SIZE);
        memset(pointers, 0, sizeof(pointers));

        for (uint_32 index = arguments->argc; index > 0; index--) {
                uint_32 argument = index - 1U;
                uint_32 bytes = arguments->lengths[argument] + 1U;

                if (cursor < bytes) {
                        result = -E2BIG;
                        goto out;
                }
                cursor -= bytes;
                memcpy(page + cursor, arguments->values[argument], bytes);
                pointers[argument] = USER_STACK3_VADDR + cursor;
        }

        cursor &= ~(sizeof(uint_32) - 1U);
        pointer_bytes = (arguments->argc + 1U) * sizeof(uint_32);
        if (cursor < pointer_bytes + EXEC_STACK_HEADROOM) {
                result = -E2BIG;
                goto out;
        }
        cursor -= pointer_bytes;
        memcpy(page + cursor, pointers, pointer_bytes);

        result = vm_user_map_owned_page(mm, USER_STACK3_VADDR);
        if (result != 0)
                goto out;
        result = vm_user_write_owned(mm, USER_STACK3_VADDR, page,
                                     PAGE_SIZE);
        if (result != 0)
                goto out;

        *argv_out = USER_STACK3_VADDR + cursor;
        *stack_out = *argv_out & ~0x0fU;
out:
        free_page(MP_KERNEL, page, 1);
        return result;
}

static int exec_prepare_image(const struct exec_arguments *arguments,
                              struct mm_struct **mm_out,
                              uint_32 *entry_out,
                              uint_32 *stack_out,
                              uint_32 *argv_out)
{
        struct exec_image_plan *plan = NULL;
        struct mm_struct *mm = NULL;
        struct file *file = NULL;
        int result;
        int close_result;

        plan = kmalloc(sizeof(*plan));
        if (plan == NULL)
                return -ENOMEM;
        result = vfs_open_file(arguments->path, O_RDONLY, &file);
        if (result != 0)
                goto out;
        result = exec_build_plan(file, plan);
        if (result != 0)
                goto out;

        mm = process_create_user_mm();
        if (mm == NULL) {
                result = -ENOMEM;
                goto out;
        }
        result = exec_load_segments(file, plan, mm);
        if (result != 0)
                goto out;
        result = exec_build_stack(mm, arguments, stack_out, argv_out);
        if (result != 0)
                goto out;

        *entry_out = plan->entry;
out:
        close_result = file != NULL ? file_put(file) : 0;
        if (result == 0 && close_result < 0)
                result = close_result;
        kfree(plan);
        if (result != 0) {
                mm_release_address_space(mm);
                return result;
        }
        *mm_out = mm;
        return 0;
}

static const char *exec_image_name(const char *path)
{
        const char *slash = strrchr(path, '/');

        if (slash != NULL && slash[1] != '\0')
                return slash + 1;
        return path;
}

int_32 sys_execv(const char *path, const char *argv[])
{
        struct exec_arguments arguments;
        struct mm_struct *new_mm = NULL;
        struct mm_struct *old_mm = NULL;
        struct context_registers *context;
        uint_32 entry;
        uint_32 stack;
        uint_32 user_argv;
        unsigned long entry_flags;
        int result;

        local_irq_save(entry_flags);
        if (running_thread() == NULL || running_thread()->mm == NULL) {
                local_irq_restore(entry_flags);
                return -EPERM;
        }
        local_irq_enable();

        result = exec_copy_arguments(path, argv, &arguments);
        if (result != 0)
                goto fail;
        result = exec_prepare_image(&arguments, &new_mm, &entry, &stack,
                                    &user_argv);
        if (result != 0)
                goto release_arguments;
        result = process_commit_user_image(
            new_mm, exec_image_name(arguments.path), entry, stack,
            arguments.argc, user_argv, &old_mm);
        if (result != 0) {
                mm_release_address_space(new_mm);
                goto release_arguments;
        }

        exec_release_arguments(&arguments);
        mm_release_address_space(old_mm);
        context = (struct context_registers *)
            ((uint_32) running_thread() + PAGE_SIZE - sizeof(*context));
        local_irq_disable();
        __asm__ volatile("movl %0, %%esp; jmp intr_exit"
                         :
                         : "g"(context)
                         : "memory");
        __builtin_unreachable();

release_arguments:
        exec_release_arguments(&arguments);
fail:
        local_irq_restore(entry_flags);
        return result;
}
