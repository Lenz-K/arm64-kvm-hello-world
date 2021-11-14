#ifndef OPTEE_CLIENT_KVM_ELF_LOADER_H
#define OPTEE_CLIENT_KVM_ELF_LOADER_H

#include <stdint.h>

/**
 * Opens an ELF file for loading. This must be called before any other function.
 * @param path The absolut path and name of the ELF file to open.
 * @return 0 on success, -1 if an error occurred.
 */
int open_elf(const char *path);

/**
 * Checks whether there is another section to load or not.
 * @return 1 if there is another section to load, 0 if not.
 */
int has_next_section_to_load(void);

/**
 * Returns information for the next section to load.
 * Prior to calling this function, has_next_section_to_load() has to be called,
 * to verify whether there is another section to load or not.
 *
 * @param code The pointer to the code block to load.
 * @param memory_size The size of the code block to load in bytes.
 * @param vaddress The virtual address where the code should be loaded to.
 * @return 0 on success, -1 if an error occurred.
 */
int get_next_section_to_load(uint32_t **code, size_t *memory_size, uint64_t *vaddress);

/**
 * Returns the entry address of the program, when it is loaded into memory.
 * @return The entry address.
 */
uint64_t get_entry_address(void);

/**
 * Closes an ELF file after loading and frees allocated resources.
 */
void close_elf(void);

#endif //OPTEE_CLIENT_KVM_ELF_LOADER_H
