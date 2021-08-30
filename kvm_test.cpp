#include <sys/ioctl.h>
#include <string>
#include <stdio.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <cerrno>
#include <fcntl.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_VM_RUNS 20
#define END_SIGNAL 0xaaaaaaaa

using namespace std;

int kvm, vmfd, vcpufd;
u_int32_t slot_count = 0;
struct kvm_run *run;
char mmio_buffer[MAX_VM_RUNS];

/**
 * Execute an ioctl with the given arguments. Exit the program if there is an error.
 *
 * @param file_descriptor
 * @param request
 * @param argument
 * @param name The name of the ioctl request for error output.
 * @return The return value of the ioctl.
 */
int ioctl_exit_on_error(int file_descriptor, unsigned long request, void *argument, string name) {
    int ret = ioctl(file_descriptor, request, argument);
    if (ret < 0) {
        printf("System call '%s' failed: %s\n", name.c_str(), strerror(errno));
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
uint8_t *allocate_memory_to_vm(size_t memory_len, uint64_t guest_addr, uint32_t flags = 0) {
    void *void_mem = mmap(NULL, memory_len, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    uint8_t *mem = static_cast<uint8_t *>(void_mem);
    if (!mem) {
        printf("Error while allocating guest memory: %s\n", strerror(errno));
        exit(-1);
    }

    struct kvm_userspace_memory_region region = {
            .slot = slot_count,
            .flags = flags,
            .guest_phys_addr = guest_addr,
            .memory_size = memory_len,
            .userspace_addr = (uint64_t) mem,
            };
    slot_count++;
    ioctl_exit_on_error(vmfd, KVM_SET_USER_MEMORY_REGION, &region, "KVM_SET_USER_MEMORY_REGION");
    return mem;
}

/**
 * Handles a MMIO Exit from KVM_RUN.
 *
 * @param buffer_index The index to write to in the MMIO buffer.
 */
void mmio_exit_handler(int buffer_index, bool *done) {
    printf("Is write: %d\n", run->mmio.is_write);

    if (run->mmio.is_write) {
        printf("Length: %d\n", run->mmio.len);
        uint64_t data = 0;
        for (int j = 0; j < run->mmio.len; j++) {
            data = data | run->mmio.data[j]<<8*j;
        }

        if ((uint32_t )data == END_SIGNAL) {
            *done = true;
        } else {
            mmio_buffer[buffer_index] = data;
        }
        printf("Guest wrote %08lX to 0x%08llX\n", data, run->mmio.phys_addr);
    }
}

void system_event_exit_handler() {
    printf("Type: %d\n", run->system_event.type);
}

/**
 * This is a KVM test program for ARM64.
 * As a starting point, this KVM test program for x86 was used: https://lwn.net/Articles/658512/
 * It is explained here: https://lwn.net/Articles/658511/
 * To change the code from x86 to ARM64 the KVM API Documentation was used: https://www.kernel.org/doc/html/latest/virt/kvm/api.html
 */
int main() {
    int ret;
    uint8_t *mem;
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
    vmfd = ioctl_exit_on_error(kvm, KVM_CREATE_VM, (unsigned long) 0, "KVM_CREATE_VM");

    printf("Setting up memory\n");
    /*
     * MEMORY MAP
     *
     * Start      | Name  | Description
     * -----------+-------+------------
     * 0x10000000 | MMIO  |
     * 0x0401F000 | Stack | grows downwards, so the SP is initially 0x04020000
     * 0x04010000 | Heap  | grows upward
     * 0x04000000 | RAM   |
     * 0x00000000 | ROM   |
     *
     */
    check_vm_extension(KVM_CAP_USER_MEMORY, "KVM_CAP_USER_MEMORY");

    printf("Loading ROM\n");
    mem = allocate_memory_to_vm(0x1000, 0x0);
    FILE *fp = fopen("/home/lenz/kvm-test/bare-metal-arm64/rom.dat", "rb");
    if (fp == NULL) {
        printf("Could not open file: %s\n", strerror(errno));
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    long n_instructions = ftell(fp) / 4; // One instruction is 4 Bytes long
    rewind(fp);

    uint8_t instruction[4];
    uint32_t rom_code[n_instructions];
    // Read one instruction after another
    for (int i = 0; i < n_instructions; i++) {
        fread(instruction, sizeof(instruction[0]), 4, fp);
        rom_code[i] = instruction[3]<<3*8 | instruction[2]<<2*8 | instruction[1]<<8 | instruction[0]; // We need to reorder the Bytes
        printf("%08X\n", rom_code[i]);
    }
    fclose(fp);
    // Copy the code into the VM memory
    memcpy(mem, rom_code, sizeof(rom_code));

    printf("\nLoading RAM\n");
    mem = allocate_memory_to_vm(0x1000, 0x04000000);
    fp = fopen("/home/lenz/kvm-test/bare-metal-arm64/ram.dat", "rb");
    if (fp == NULL) {
        printf("Could not open file: %s\n", strerror(errno));
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    n_instructions = ftell(fp) / 4;
    rewind(fp);

    uint32_t ram_code[n_instructions];
    for (int i = 0; i < n_instructions; i++) {
        fread(instruction, sizeof(instruction[0]), 4, fp);
        ram_code[i] = instruction[3]<<3*8 | instruction[2]<<2*8 | instruction[1]<<8 | instruction[0];
        printf("%08X\n", ram_code[i]);
    }
    fclose(fp);
    memcpy(mem, ram_code, sizeof(ram_code));

    mem = allocate_memory_to_vm(0x1000, 0x04010000);
    mem = allocate_memory_to_vm(0x1000, 0x0401F000);
    check_vm_extension(KVM_CAP_READONLY_MEM, "KVM_CAP_READONLY_MEM");
    mem = allocate_memory_to_vm(0x1000, 0x10000000, KVM_MEM_READONLY);

    /* Create a virtual CPU and receive its file descriptor */
    printf("Creating VCPU\n");
    vcpufd = ioctl_exit_on_error(vmfd, KVM_CREATE_VCPU, (unsigned long) 0, "KVM_CREATE_VCPU");

    /* Get CPU information for VCPU init*/
    printf("Retrieving physical CPU information\n");
    struct kvm_vcpu_init preferred_target;
    ioctl_exit_on_error(vmfd, KVM_ARM_PREFERRED_TARGET, &preferred_target, "KVM_ARM_PREFERRED_TARGET");

    /* Initialize VCPU */
    printf("Initializing VCPU\n");
    ioctl_exit_on_error(vcpufd, KVM_ARM_VCPU_INIT, &preferred_target, "KVM_ARM_VCPU_INIT");

    /* Map the shared kvm_run structure and following data. */
    ret = ioctl_exit_on_error(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL, "KVM_GET_VCPU_MMAP_SIZE");
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        printf("KVM_GET_VCPU_MMAP_SIZE unexpectedly small");
    void *void_mem = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
    run = static_cast<kvm_run *>(void_mem);
    if (!run)
        printf("Error while mmap vcpu");

    /* Repeatedly run code and handle VM exits. */
    printf("Running code\n");
    bool done = false;
    for (int i = 0; i < MAX_VM_RUNS && !done; i++) {
        printf("\nLoop %d:\n", i+1);
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret < 0) {
            printf("System call 'KVM_RUN' failed: %d - %s\n", errno, strerror(errno));
            printf("Error Numbers: EINTR=%d; ENOEXEC=%d; ENOSYS=%d; EPERM=%d\n", EINTR, ENOEXEC, ENOSYS, EPERM);
            return ret;
        }

        switch (run->exit_reason) {
            case KVM_EXIT_HLT:
                printf("KVM_EXIT_HLT\n");
                done = true;
                break;
            case KVM_EXIT_IO:
                printf("KVM_EXIT_IO\n");
                break;
            case KVM_EXIT_MMIO:
                printf("KVM_EXIT_MMIO\n");
                mmio_exit_handler(i, &done);
                break;
            case KVM_EXIT_SYSTEM_EVENT:
                printf("KVM_EXIT_SYSTEM_EVENT\n");
                system_event_exit_handler();
                break;
            case KVM_EXIT_INTR:
                printf("KVM_EXIT_INTR\n");
                break;
            case KVM_EXIT_FAIL_ENTRY:
                printf("KVM_EXIT_FAIL_ENTRY\n");
                break;
            case KVM_EXIT_INTERNAL_ERROR:
                printf("KVM_EXIT_INTERNAL_ERROR\n");
                break;
            default:
                printf("KVM_EXIT other\n");
        }
    }

    printf("\nVM MMIO Output:\n");
    for(int i = 0; mmio_buffer[i] != '\0'; i++) {
        printf("%c", mmio_buffer[i]);
    }

    return 0;
}
