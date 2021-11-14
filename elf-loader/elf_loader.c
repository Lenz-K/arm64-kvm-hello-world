#include <stdio.h>
#include <stdlib.h>

#include "elf_loader.h"

// Constant for loadable segment
#define PT_LOAD 0x00000001

// The layout of the headers is explained here: https://en.wikipedia.org/wiki/Executable_and_Linkable_Format#File_header
// Absolut offsets in an ELF file
#define BIT_ARCH 0x4
#define ENTRY 0x18
#define P_H_ENT_SIZE 0x36

// Relative offsets to program header entries in an ELF file
#define OFFSET_P_OFFSET 0x8
#define OFFSET_P_FILE_SIZE 0x20

// The ELF file
FILE *fp;
// The current position in the ELF file
int byte_index = 0;

// The entry address of the program when loaded into memory
uint64_t entry_address;

// The program header entries
int p_hdr_n;
int *do_load;
uint64_t *offset;
uint64_t *vaddr;
size_t *filesz;
size_t *memsz;
uint32_t **code_blocks;

// The current section index
int section = -1;
int has_next = 0;

/**
 * Read from the ELF file.
 * This keeps the current byte_index up to date.
 *
 * @param mem The pointer where to store the read bytes
 * @param size The size of the blocks to read
 * @param n The number of blocks to read
 */
void read(void *mem, size_t size, int n) {
    int ret = fread(mem, size, n, fp);
    int read_n = ret;
    while (read_n < n && ret > 0) {
        ret = fread(mem, size, n, fp);
        read_n += ret;
    }
    byte_index += read_n * size;
}

/**
 * Fast forward in the ELF file to the specified absolut offset.
 *
 * @param off The offset to go to.
 */
void to(int off) {
    int forward = off - byte_index;
    uint8_t byte[forward];
    read(byte, sizeof(byte[0]), forward);
}

/**
 * Pases the program header and caches the fields we will need.
 *
 * @param phoff The offset of the program header in the ELF file.
 * @param phentsize The size of one entry in the program header.
 */
void parse_program_header(uint64_t phoff, uint16_t phentsize) {
    do_load = (int *) malloc(p_hdr_n * sizeof(int));
    offset = (uint64_t *) malloc(p_hdr_n * sizeof(uint64_t));
    vaddr = (uint64_t *) malloc(p_hdr_n * sizeof(uint64_t));
    filesz = (size_t *) malloc(p_hdr_n * sizeof(size_t));
    memsz = (size_t *) malloc(p_hdr_n * sizeof(size_t));
    
    for (int i = 0; i < p_hdr_n; i++) {
        to(phoff + i * phentsize);
        
        uint32_t p_type;
        read(&p_type, sizeof(p_type), 1);
        do_load[i] = p_type == PT_LOAD;
        
        if (do_load[i]) {
            to(phoff + i * phentsize + OFFSET_P_OFFSET);
            
            uint64_t p_offset;
            read(&p_offset, sizeof(p_offset), 1);
            offset[i] = p_offset;
        
            uint64_t p_vaddr;
            read(&p_vaddr, sizeof(p_vaddr), 1);
            vaddr[i] = p_vaddr;
        
            to(phoff + i * phentsize + OFFSET_P_FILE_SIZE);
            
            uint64_t p_filesz;
            read(&p_filesz, sizeof(p_filesz), 1);
            filesz[i] = p_filesz;
        
            uint64_t p_memsz;
            read(&p_memsz, sizeof(p_memsz), 1);
            memsz[i] = p_memsz;
        }
    }

    code_blocks = (uint32_t **) malloc(p_hdr_n * sizeof(uint32_t *));
    for (int i = 0; i < p_hdr_n; i++) {
        code_blocks[i] = (uint32_t *) malloc(memsz[i]);
    }
}

int open_elf(const char *path) {
    printf("Opening ELF file\n");
    fp = fopen(path, "rb");

    to(BIT_ARCH);
    uint8_t flag;
    read(&flag, sizeof(flag), 1);
    if (flag != 2) {
        printf("Not a 64-bit ELF object.\n");
        return -1;
    }
    read(&flag, sizeof(flag), 1);
    if (flag != 1) {
        printf("Not little endian.\n");
        return -1;
    }

    to(ENTRY);
    read(&entry_address, sizeof(entry_address), 1);

    uint64_t phoff;
    read(&phoff, sizeof(phoff), 1);

    to(P_H_ENT_SIZE);
    uint16_t phentsize;
    read(&phentsize, sizeof(phentsize), 1);

    uint16_t phnum;
    read(&phnum, sizeof(phnum), 1);
    p_hdr_n = phnum;
    printf("It contains %d sections\n", p_hdr_n);

    parse_program_header(phoff, phentsize);

    return 0;
}

int has_next_section_to_load() {
    do {
        section++;
        has_next = do_load[section];
        if (has_next) {
            printf("Section %d needs to be loaded\n", section);
        } else {
            printf("Section %d does not need to be loaded\n", section);
        }
    } while (!has_next && section + 1 < p_hdr_n);

    return has_next;
}

int get_next_section_to_load(uint32_t **code, size_t *memory_size, uint64_t *vaddress) {
    if (!has_next) {
        printf("No more section available. "
               "Check availability with has_next_section_to_load() prior to calling this method.\n");
        return -1;
    }

    code_blocks[section] = (uint32_t *) malloc(memsz[section]);
    to(offset[section]);
    read(code_blocks[section], sizeof(uint32_t), filesz[section] / sizeof(uint32_t));

    *code = code_blocks[section];
    *memory_size = memsz[section];
    *vaddress = vaddr[section];

    return 0;
}

uint64_t get_entry_address() {
    return entry_address;
}

void close_elf() {
    printf("Closing ELF file\n");
    fclose(fp);
    
    free(do_load);
    free(offset);
    free(vaddr);
    free(filesz);
    free(memsz);
    
    for (int i = 0; i < p_hdr_n; i++) {
        free(code_blocks[i]);
    }
    free(code_blocks);
}

