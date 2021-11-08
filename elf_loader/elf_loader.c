#include "elf_loader.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <gelf.h>

int fd;
Elf *e;
Elf64_Phdr *phdr;
size_t phdrnum;
int section = -1;
int has_next = 0;

int open_elf(char *filename) {
    printf("Opening ELF file.\n");
    if (elf_version(EV_CURRENT) == EV_NONE) {
        printf("ELF library initialization failed.\n");
        return -1;
    }

    if ((fd = open(filename, O_RDONLY, 0)) < 0) {
        printf("Cannot open '%s': %s\n", filename, strerror(errno));
        return -1;
    }

    if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
        printf("elf_begin() failed.\n");
        return -1;
    }

    if (elf_kind(e) != ELF_K_ELF) {
        printf("Not an ELF object.\n");
        return -1;
    }

    if (gelf_getclass(e) != ELFCLASS64) {
        printf("Not a 64-bit ELF object.\n");
        return -1;
    }

    if (elf_getphdrnum(e, &phdrnum) != 0) {
        printf("elf_getphdrnum() failed.\n");
        return -1;
    }
    printf("It contains %zu sections.\n", phdrnum);

    if ((phdr = elf64_getphdr(e)) == NULL) {
        printf("elf64_getphdr() failed.\n");
        return -1;
    }

    return 0;
}

int has_next_section_to_load() {
    int do_load = 0;
    while (!do_load && section + 1 < ((int) phdrnum)) {
        section++;
        do_load = phdr[section].p_type == PT_LOAD;

        has_next = do_load;
        if (do_load) {
            printf("\nSection %d needs to be loaded.\n", section);
        } else {
            printf("\nSection %d does not need to be loaded.\n", section);
        }
    }

    return has_next;
}

int get_next_section_to_load(Elf64_Word **code, size_t *memsz, Elf64_Addr *vaddress) {
    if (!has_next) {
        printf("No more section available. "
               "Check availability with has_next_section_to_load() prior to calling this method.\n");
        return -1;
    }

    Elf_Data *data = elf_getdata_rawchunk(e, phdr[section].p_offset, phdr[section].p_filesz, ELF_T_WORD);
    *code = data->d_buf;
    *memsz = data->d_size;
    *vaddress = phdr[section].p_vaddr;

    return 0;
}

Elf64_Addr get_entry_address() {
    Elf64_Ehdr *ehdr;

    if ((ehdr = elf64_getehdr(e)) == NULL) {
        printf("elf64_getehdr() failed.\n");
        return -1;
    }

    return ehdr->e_entry;
}

int print_elf_info() {
    printf("\nPrinting information about the ELF file.\n");
    printf("Executable Header:\n");
    Elf64_Ehdr *ehdr;

    if ((ehdr = elf64_getehdr(e)) == NULL) {
        printf("elf64_getehdr() failed.\n");
        return -1;
    }

    printf("e_entry: 0x%08lX\n", ehdr->e_entry);
    printf("e_phoff: 0x%08lX\n", ehdr->e_phoff);
    printf("e_shoff: 0x%08lX\n", ehdr->e_shoff);
    printf("e_ehsize: 0x%08X\n", ehdr->e_ehsize);
    printf("e_phentsize: 0x%08X\n", ehdr->e_phentsize);
    printf("e_shentsize: 0x%08X\n", ehdr->e_shentsize);

    size_t shdrnum, shdrstrndx;
    if (elf_getshdrnum(e, &shdrnum) != 0) {
        printf("elf_getshdrnum() failed.\n");
        return -1;
    }
    printf("shdrnum: %zu\n", shdrnum);

    if (elf_getshdrstrndx(e, &shdrstrndx) != 0) {
        printf("elf_getshdrstrndx() failed.\n");
        return -1;
    }
    printf("shdrstrndx: %zu\n", shdrstrndx);
    printf("phdrnum: %zu\n", phdrnum);

    printf("\nProgram Header Table:");

    for (int i = 0; i < phdrnum; i++) {
        printf("\nPHdr %d\n", i);
        printf("p_type: 0x%08X ", phdr[i].p_type);
        switch (phdr[i].p_type) {
            case PT_LOAD:
                printf("LOAD\n");
                break;
            case PT_DYNAMIC:
                printf("DYNAMIC\n");
                break;
            case PT_GNU_STACK:
                printf("STACK\n");
                break;
            default:
                printf("UNKNOWN\n");
        }

        printf("p_offset: 0x%08lX\n", phdr[i].p_offset);
        printf("p_vaddr: 0x%08lX\n", phdr[i].p_vaddr);
        printf("p_paddr: 0x%08lX\n", phdr[i].p_paddr);
        printf("p_filesz: 0x%08lX\n", phdr[i].p_filesz);
        printf("p_memsz: 0x%08lX\n", phdr[i].p_memsz);

        printf("p_flags:");
        if (phdr[i].p_flags & PF_X) printf(" execute");
        if (phdr[i].p_flags & PF_R) printf(" read");
        if (phdr[i].p_flags & PF_W) printf(" write");
        printf("\n");

        printf("p_align: 0x%08lX\n", phdr[i].p_align);
    }

    return 0;
}

void close_elf() {
    printf("Closing ELF file.\n");
    elf_end(e);
    close(fd);
}

