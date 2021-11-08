#ifndef OPTEE_CLIENT_KVM_ELF_LOADER_H
#define OPTEE_CLIENT_KVM_ELF_LOADER_H

#include <libelf.h>

/**
 * Opens an ELF file for loading. This must be called before any other function.
 * @param filename The name of the ELF file to open.
 * @return 0 on success, -1 if an error occurred.
 */
int open_elf(char *filename);

/**
 * Checks whether there is another section to load or not.
 * @return 1 if there is another section to load, 0 if not.
 */
int has_next_section_to_load();

/**
 * Returns information for the next section to load.
 * Prior to calling this function, has_next_section_to_load() has to be called,
 * to verify whether there is another section to load or not.
 *
 * @param code The pointer to the code block to load.
 * @param memsz The size of the code block to load in bytes.
 * @param vaddress The virtual address where the code should be loaded to.
 * @return 0 on success, -1 if an error occurred.
 */
int get_next_section_to_load(Elf64_Word **code, size_t *memsz, Elf64_Addr *vaddress);

/**
 * Returns the entry address of the program, when it is loaded into memory.
 * @return The entry address or -1 if an error occurred.
 */
Elf64_Addr get_entry_address();

/**
 * Prints information stored in the opened ELF file.
 *
 * @return 0 on success, -1 if an error occurred.
 */
int print_elf_info();

/**
 * Closes an ELF file after loading and frees allocated resources.
 */
void close_elf();

#endif //OPTEE_CLIENT_KVM_ELF_LOADER_H
