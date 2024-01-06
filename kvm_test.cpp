#include <sys/ioctl.h>
#include <string>
#include <linux/kvm.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstddef>
#include <cstring>
#include <sys/mman.h>
#include <stdarg.h>
#include "elf-loader/elf_loader.h"

#define MAX_VM_RUNS 20
#define N_MEMORY_MAPPINGS 2
#define MEMORY_BLOCK_SIZE 0x8000

using namespace std;

int kvm, vmfd, vcpufd;
struct kvm_run *run;
u_int32_t memory_slot_count = 0;

// Memory mappings between host and guest
struct memory_mapping {
    uint64_t guest_phys_addr;
    size_t memory_size;
    uint64_t *userspace_addr;
};
memory_mapping memory_mappings[N_MEMORY_MAPPINGS];

int mmio_buffer_index = 0;
char mmio_buffer[MAX_VM_RUNS];

/**
 * Execute an ioctl with the given arguments. Exit the program if there is an error.
 *
 * @param file_descriptor
 * @param request
 * @param name The name of the ioctl request for error output.
 * @return The return value of the ioctl.
 */
int ioctl_exit_on_error(int file_descriptor, unsigned long request, string name, ...) {
    va_list ap;
    va_start(ap, name);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    
    int ret = ioctl(file_descriptor, request, arg);
    if (ret < 0) {
        printf("System call '%s' failed: %s - %d\n", name.c_str(), strerror(errno), ret);
        exit(ret);
    }
    return ret;
}

/**
 * Checks the availability of a KVM extension. Exits on errors and if the extension is not available.
 *
 * @param extension The extension identifier to check for.
 * @param name The name of the extension for log statements.
 * @return The return value of the involved ioctl.
 */
int check_vm_extension(int extension, string name) {
    int ret = ioctl(vmfd, KVM_CHECK_EXTENSION, extension);
    if (ret < 0) {
        printf("System call 'KVM_CHECK_EXTENSION' failed: %s\n", strerror(errno));
        exit(ret);
    }
    if (ret == 0) {
        printf("Extension '%s' not available\n", name.c_str());
        exit(-1);
    }
    return ret;
}

/**
 * Allocates memory and assigns it to the VM as guest memory.
 *
 * @param memory_len The length of the memory that shall be allocated.
 * @param guest_addr The address of the memory in the guest.
 * @return A pointer to the allocated memory.
 */
uint64_t *allocate_memory_to_vm(size_t memory_len, uint64_t guest_addr, uint32_t flags = 0) {
    void *void_mem = mmap(NULL, memory_len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    uint64_t *mem = static_cast<uint64_t *>(void_mem);
    if (!mem) {
        printf("Error while allocating guest memory: %s\n", strerror(errno));
        exit(-1);
    }

    struct kvm_userspace_memory_region region = {
            .slot = memory_slot_count,
            .flags = flags,
            .guest_phys_addr = guest_addr,
            .memory_size = memory_len,
            .userspace_addr = (uint64_t) mem,
            };
    memory_slot_count++;
    ioctl_exit_on_error(vmfd, KVM_SET_USER_MEMORY_REGION, "KVM_SET_USER_MEMORY_REGION", &region);
    return mem;
}

/**
 * Finds the memory mapping for the specified target_addr.
 *
 * @param target_addr The guest address that will be searched for in the memory mappings.
 * @return Returns the index of the memory mapping or -1 if no mapping was found.
 */
int find_mapping_for_section(uint64_t target_addr) {
    // Iterate over the memory mappings from high addresses to lower addresses.
    for (int i = N_MEMORY_MAPPINGS-1; i >= 0; i--) {
        // As soon as one mapping has a lower guest address as the target address, the right mapping is found.
        if (memory_mappings[i].guest_phys_addr <= target_addr) {
            return i;
        }
    }

    return -1;
}

/**
 * Copies the code into the memory of the specified memory mapping.
 *
 * @param code The code blok that will be copied into the VM memory.
 * @param memsz The size of the code block.
 * @param target_addr The VM memory address that the code will be copied to.
 * @param mmi The index of the memory mapping that will be used for copying.
 * @return Returns the index of the memory mapping or -1 if no mapping was found.
 */
int copy_section_into_memory(uint32_t *code, size_t memsz, uint64_t target_addr, int mmi) {
    // There can be an offset between memory mapping and the target address.
    uint64_t offset = target_addr - memory_mappings[mmi].guest_phys_addr;

    // If the offset plus the code size is bigger than the memory mapping size, do nothing.
    if (offset + memsz > memory_mappings[mmi].memory_size) {
        printf("Memory mapping too small. Mapping offset: 0x%08lX - Mapping size: 0x%08lX\n", offset, memory_mappings[mmi].memory_size);
        return -1;
    }

    // Copy the code into the VM memory
    memcpy(memory_mappings[mmi].userspace_addr + offset, code, memsz);
    printf("Section loaded. Host address: %p - Guest address: 0x%08lX\n", memory_mappings[mmi].userspace_addr + offset, target_addr);
    return 0;
}

/**
 * Copies the required sections of the ELF file into the memory of the VM.
 *
 * @return 0 on success, -1 if an error occurred.
 */
int copy_elf_into_memory() {
    string elf_name = "bare-metal-aarch64/hello_world.elf";
    // Open the ELF file that will be loaded into memory
    if (open_elf(elf_name.c_str()) != 0)
        return -1;

    uint32_t *code;
    size_t memsz;
    uint64_t target_addr;
    // Iterate over the segments in the ELF file and load them into the memory of the VM
    while (has_next_section_to_load()) {
        if (get_next_section_to_load(&code, &memsz, &target_addr) < 0)
            return -1;
        int mmi = find_mapping_for_section(target_addr);
        if (mmi < 0)
            return -1;
        if (copy_section_into_memory(code, memsz, target_addr, mmi) < 0)
            return -1;
    }

    close_elf();
    return 0;
}

/**
 * Handles a MMIO exit from KVM_RUN.
 */
void mmio_exit_handler() {
    printf("Is Write: %d\n", run->mmio.is_write);

    if (run->mmio.is_write) {
        printf("Length: %d\n", run->mmio.len);
        uint64_t data = 0;
        for (int j = 0; j < run->mmio.len; j++) {
            data |= run->mmio.data[j]<<8*j;
        }

        mmio_buffer[mmio_buffer_index] = data;
        mmio_buffer_index++;
        printf("Guest wrote 0x%08lX to 0x%08llX\n", data, run->mmio.phys_addr);
    }
}

/**
 * Prints the reason of a system event exit from KVM_RUN.
 */
void print_system_event_exit_reason() {
    switch (run->system_event.type) {
    case KVM_SYSTEM_EVENT_SHUTDOWN:
        printf("Cause: Shutdown\n");
        break;
    case KVM_SYSTEM_EVENT_RESET:
        printf("Cause: Reset\n");
        break;
    case KVM_SYSTEM_EVENT_CRASH:
        printf("Cause: Crash\n");
        break;
    }
}

/**
 * Closes a file descriptor and therefore frees its resources.
 */
void close_fd(int fd) {
    int ret = close(fd);
    if (ret == -1)
        printf("Error while closing file: %s\n", strerror(errno));
}

/**
 * This is a KVM test program for AArch64.
 * As a starting point, this KVM test program for x86 was used: https://lwn.net/Articles/658512/
 * It is explained here: https://lwn.net/Articles/658511/
 * To change the code from x86 to AArch64 the KVM API Documentation (https://www.kernel.org/doc/html/latest/virt/kvm/api.html) and the QEMU source code were used.
 */
int main() {
    int ret;
    uint64_t *mem;
    size_t mmap_size;

    /* Get the KVM file descriptor */
    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm < 0) {
        printf("Cannot open '/dev/kvm': %s", strerror(errno));
        return kvm;
    }

    /* Make sure we have the stable version of the API */
    ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
    if (ret < 0) {
        printf("System call 'KVM_GET_API_VERSION' failed: %s", strerror(errno));
        return ret;
    }
    if (ret != 12) {
        printf("expected KVM API Version 12 got: %d", ret);
        return -1;
    }

    /* Create a VM and receive the VM file descriptor */
    printf("Creating VM\n");
    vmfd = ioctl_exit_on_error(kvm, KVM_CREATE_VM, "KVM_CREATE_VM", (unsigned long) 0);

    printf("Setting up memory\n");
    /*
     * MEMORY MAP
     * One memory block of 0x1000 B will be assigned to every part of the memory:
     *
     * Start      | Name  | Description
     * -----------+-------+------------
     * 0x00000000 | ROM   |
     * 0x04000000 | RAM   |
     * 0x04010000 | Heap  | increases
     * 0x0401F000 | Stack | decreases, so the stack pointer is initially 0x04020000
     * 0x10000000 | MMIO  |
     */
    check_vm_extension(KVM_CAP_USER_MEMORY, "KVM_CAP_USER_MEMORY");

    /* ROM Memory */
    memory_mappings[0].guest_phys_addr = 0x0;
    memory_mappings[0].memory_size = MEMORY_BLOCK_SIZE;
    mem = allocate_memory_to_vm(memory_mappings[0].memory_size, memory_mappings[0].guest_phys_addr);
    memory_mappings[0].userspace_addr = mem;

    /* RAM Memory */
    memory_mappings[1].guest_phys_addr = 0x04000000;
    memory_mappings[1].memory_size = MEMORY_BLOCK_SIZE;
    mem = allocate_memory_to_vm(memory_mappings[1].memory_size, memory_mappings[1].guest_phys_addr);
    memory_mappings[1].userspace_addr = mem;

    ret = copy_elf_into_memory();
    if (ret < 0)
        return ret;

    /* Heap Memory */
    mem = allocate_memory_to_vm(MEMORY_BLOCK_SIZE * 2, 0x04010000);
    /* Stack Memory */
    // mem = allocate_memory_to_vm(MEMORY_BLOCK_SIZE, 0x04020000);

    /* MMIO Memory */
    check_vm_extension(KVM_CAP_READONLY_MEM, "KVM_CAP_READONLY_MEM"); // This will cause a write to 0x10000000, to result in a KVM_EXIT_MMIO.
    mem = allocate_memory_to_vm(MEMORY_BLOCK_SIZE, 0x10000000, KVM_MEM_READONLY);

    /* Create a virtual CPU and receive its file descriptor */
    printf("Creating VCPU\n");
    vcpufd = ioctl_exit_on_error(vmfd, KVM_CREATE_VCPU, "KVM_CREATE_VCPU", (unsigned long) 0);

    /* Get CPU information for VCPU init */
    printf("Retrieving physical CPU information\n");
    struct kvm_vcpu_init preferred_target;
    ioctl_exit_on_error(vmfd, KVM_ARM_PREFERRED_TARGET, "KVM_ARM_PREFERRED_TARGET", &preferred_target);

    /* Enable the PSCI v0.2 CPU feature, to be able to shut down the VM */
    check_vm_extension(KVM_CAP_ARM_PSCI_0_2, "KVM_CAP_ARM_PSCI_0_2");
    preferred_target.features[0] |= 1 << KVM_ARM_VCPU_PSCI_0_2;

    /* Initialize VCPU */
    printf("Initializing VCPU\n");
    ioctl_exit_on_error(vcpufd, KVM_ARM_VCPU_INIT, "KVM_ARM_VCPU_INIT", &preferred_target);

    /* Map the shared kvm_run structure and following data. */
    ret = ioctl_exit_on_error(kvm, KVM_GET_VCPU_MMAP_SIZE, "KVM_GET_VCPU_MMAP_SIZE", NULL);
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        printf("KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    void *void_mem = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
    run = static_cast<kvm_run *>(void_mem);
    if (!run)
        printf("Error while mmap vcpu");
        
    /* Set program counter to entry address */
    check_vm_extension(KVM_CAP_ONE_REG, "KVM_CAP_ONE_REG");
    uint64_t pc_index = offsetof(struct kvm_regs, regs.pc) / sizeof(__u32);
    uint64_t pc_id = KVM_REG_ARM64 | KVM_REG_SIZE_U64 | KVM_REG_ARM_CORE | pc_index;
    uint64_t entry_addr = get_entry_address();
    printf("Setting program counter to entry address 0x%08lX\n", entry_addr);
    struct kvm_one_reg pc = {.id = pc_id, .addr = (uint64_t)&entry_addr};
    ret = ioctl_exit_on_error(vcpufd, KVM_SET_ONE_REG, "KVM_SET_ONE_REG", &pc);
    if (ret < 0)
        return ret;

    /* Repeatedly run code and handle VM exits. */
    printf("Running code\n");
    bool shut_down = false;
    for (int i = 0; i < MAX_VM_RUNS && !shut_down; i++) {
        printf("\nKVM_RUN Loop %d:\n", i+1);
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret < 0) {
            printf("System call 'KVM_RUN' failed: %d - %s\n", errno, strerror(errno));
            printf("Error Numbers: EINTR=%d; ENOEXEC=%d; ENOSYS=%d; EPERM=%d\n", EINTR, ENOEXEC, ENOSYS, EPERM);
            return ret;
        }

        switch (run->exit_reason) {
            case KVM_EXIT_MMIO:
                printf("Exit Reason: KVM_EXIT_MMIO\n");
                mmio_exit_handler();
                break;
            case KVM_EXIT_SYSTEM_EVENT:
                // This happens when the VCPU has done a HVC based PSCI call.
                printf("Exit Reason: KVM_EXIT_SYSTEM_EVENT\n");
                print_system_event_exit_reason();
                shut_down = true;
                break;
            case KVM_EXIT_INTR:
                printf("Exit Reason: KVM_EXIT_INTR\n");
                break;
            case KVM_EXIT_FAIL_ENTRY:
                printf("Exit Reason: KVM_EXIT_FAIL_ENTRY\n");
                break;
            case KVM_EXIT_INTERNAL_ERROR:
                printf("Exit Reason: KVM_EXIT_INTERNAL_ERROR\n");
                break;
            default:
                printf("Exit Reason: other\n");
        }
    }

    printf("\nVM MMIO Output:\n");
    for(int i = 0; i < mmio_buffer_index; i++) {
        printf("%c", mmio_buffer[i]);
    }

    close_fd(vcpufd);
    close_fd(vmfd);
    close_fd(kvm);

    return 0;
}
