#include <elf.h>
#include <sys/exec.h>
#include <sys/memory.h>

#include <fs/fs.h>
extern void intr_exit(void);


static int_32 load_elf_file(const char *pathname)
{
    int_32 fd = sys_open(pathname, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    // 1. read elf header
    uint_8 *buf = sys_malloc(sizeof(Elf32_Ehdr));
    if (buf == NULL) {
        return -1;
    }
    sys_read(fd, buf, sizeof(Elf32_Ehdr));
    Elf32_Ehdr *elf_header = (Elf32_Ehdr *) buf;
    if (elf_header->e_ident[0] == EI_MAG0 &&
        elf_header->e_ident[1] == EI_MAG1 &&
        elf_header->e_ident[2] == EI_MAG2 &&
        elf_header->e_ident[3] == EI_MAG3) {

        Elf32_Off  phoff = elf_header->e_phoff;     // offset of file
        Elf32_Half phsz  = elf_header->e_phentsize; // program header entry size
        Elf32_Half phnum = elf_header->e_phnum;     // program header number


    } else {
        return -1;
    }



    sys_close(fd);
    return 0;
}

int_32 sys_execv(const char *path, const char *argv[])
{
    load_elf_file(path);
    return 0;
}
