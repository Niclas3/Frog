#include <const.h>
#include <debug.h>
#include <elf.h>
#include <math.h>
#include <string.h>
#include <sys/exec.h>
#include <sys/memory.h>
#include <sys/threads.h>

#include <fs/fs.h>
extern void intr_exit(void);
static bool load_code(int_32 fd, uint_32 virtaddr, uint_32 offset, uint_32 size)
{
    uint_32 vaddr_first_page = virtaddr & 0xfffff000;
    uint_32 size_fpage = PG_SIZE - (virtaddr & 0x00000fff);
    uint_32 occupy_page_cnt = 0;  // in page
    if (size > size_fpage) {
        uint_32 left_size = size - size_fpage;
        occupy_page_cnt = DIV_ROUND_UP(left_size, PG_SIZE) + 1;
    } else {
        occupy_page_cnt = 1;
    }

    uint_32 page_idx = 0;
    uint_32 vaddr_page = vaddr_first_page;
    while (page_idx < occupy_page_cnt) {
        uint_32 *pde = pde_ptr(vaddr_page);
        uint_32 *pte = pte_ptr(vaddr_page);

        if (!(*pde & PG_P_SET) || !(*pte & PG_P_SET)) {
            if (NULL == malloc_page_with_vaddr(MP_USER, vaddr_page)) {
                return false;
            }
        }
        vaddr_page += PG_SIZE;
        page_idx++;
    }
    sys_lseek(fd, offset, SEEK_SET);
    sys_read(fd, (void *) virtaddr, size);
    return true;
}

static int_32 load_elf_file(const char *pathname)
{
    int_32 ret = -1;
    int_32 fd = sys_open(pathname, O_RDONLY);
    if (fd == -1) {
        ret = -1;
        return ret;
    }
    // 1. read elf header
    Elf32_Off phoff;
    Elf32_Half phsz;
    Elf32_Half phnum;

    uint_8 *buf = sys_malloc(sizeof(Elf32_Ehdr));
    if (buf == NULL) {
        ret = -1;
        return ret;
    }
    sys_read(fd, buf, sizeof(Elf32_Ehdr));


    Elf32_Ehdr *elf_header = (Elf32_Ehdr *) buf;
    if (elf_header->e_ident[0] == ELFMAG0 &&
        elf_header->e_ident[1] == ELFMAG1 &&
        elf_header->e_ident[2] == ELFMAG2 &&
        elf_header->e_ident[3] == ELFMAG3) {
        phoff = elf_header->e_phoff;     // offset of file
        phsz = elf_header->e_phentsize;  // program header entry size
        phnum = elf_header->e_phnum;     // program header number
        // read program header
        Elf32_Phdr *ph_buf = sys_malloc(phsz);

        for (uint_32 idx = 0; idx < phnum; idx++) {
            memset(ph_buf, 0, phsz);
            sys_lseek(fd, phoff, SEEK_SET);
            if (sys_read(fd, ph_buf, phsz) != phsz) {
                ret = -1;
                goto done;
            }
            if (PT_LOAD == ph_buf->p_type) {
                if (!load_code(fd, ph_buf->p_vaddr, ph_buf->p_offset,
                               ph_buf->p_filesz)) {
                    ret = -1;
                    goto done;
                }
            }
            phoff += phsz;
        }

        sys_free(ph_buf);

    } else {
        ret = -1;
        return ret;
    }
    ret = elf_header->e_entry;
    goto done;
done:
    sys_close(fd);
    sys_free(buf);
    return ret;
}

int_32 sys_execv(const char *path, const char *argv[])
{
    int_32 argc;
    for (argc = 0; argv[argc]; argc++) {
    }

    int_32 entry_point = load_elf_file(path);
    if (entry_point == -1) {
        return -1;
    }
    TCB_t *cur = running_thread();
    uint_32 name_len = strlen(path);
    ASSERT(name_len < TASK_NAME_LEN);
    memcpy(cur->name, path, name_len);
    cur->name[name_len] = '\0';

    struct context_registers *intr_0_stack =
        (struct context_registers *) ((uint_32) cur + PG_SIZE -
                                      sizeof(struct context_registers));
    intr_0_stack->ebx = (int_32) argv;
    intr_0_stack->ecx = argc;
    intr_0_stack->eip = (void *) entry_point;
    intr_0_stack->esp = (void *) 0xc0000000;

    __asm__ volatile("movl %0, %%esp; jmp intr_exit" ::"g"(intr_0_stack)
                     : "memory");
    return 0;
}
