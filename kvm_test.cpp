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

using namespace std;

int kvm, vmfd, vcpufd;
u_int32_t slot_count = 0;

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
 * Checks the availability of KVM extensions.
 *
 * @param extension The extension identifier to check for.
 * @param name The name of the extension for log statements.
 * @return The return value of the involved ioctl.
 */
int check_kvm_extension(int extension, string name) {
    int ret = ioctl_exit_on_error(kvm, KVM_CHECK_EXTENSION, extension, "KVM_CHECK_EXTENSION");
    if (!ret) {
        printf("Extension %s not available", name.c_str())
        exit(-1)
    }
    return ret;
}

/**
 * Get the value of a register.
 *
 * @param id The ID of the register. They can be found in the struct 'kvm_regs'.
 * @param val The location where the result will be stored.
 * @param name The name of the register for log statements.
 * @return The return value of the involved ioctl.
 */
int get_register(uint64_t id, uint64_t *val, string name) {
    struct kvm_one_reg reg {
        .id = id,
        .addr = (uintptr_t)val,
    };
    printf("Reading register %s\n", name.c_str());
    int ret = ioctl_exit_on_error(vcpufd, KVM_GET_ONE_REG, &reg, "KVM_GET_ONE_REG");
    printf("%s: %ld\n", name.c_str(), *val);
    return ret;
}

/**
 * Set the value of a register.
 *
 * @param id The ID of the register. They can be found in the struct 'kvm_regs'.
 * @param val The location where the value to be set is stored.
 * @param name The name of the register for log statements.
 * @return The return value of the involved ioctl.
 */
int set_register(uint64_t id, uint64_t *val, string name) {
    struct kvm_one_reg reg {
        .id = id,
        .addr = (uintptr_t)val,
    };
    printf("Set register %s to %ld\n", name.c_str(), *val);
    int ret = ioctl_exit_on_error(vcpufd, KVM_SET_ONE_REG, &reg, "KVM_SET_ONE_REG");
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
            .flags = flags;
            .guest_phys_addr = guest_addr,
            .memory_size = memory_len,
            .userspace_addr = (uint64_t) mem,
            };
    slot_count++;
    ioctl_exit_on_error(vmfd, KVM_SET_USER_MEMORY_REGION, &region, "KVM_SET_USER_MEMORY_REGION");
    return mem;
}

/**
 * This is a KVM test program for ARM64.
 * As a starting point, this KVM test program for x86 was used: https://lwn.net/Articles/658512/
 * It is explained here: https://lwn.net/Articles/658511/
 * To change the code from x86 to ARM64 the KVM API Documentation was used: https://www.kernel.org/doc/html/latest/virt/kvm/api.html
 */
int main() {
    kvm_regs kvm_regs;
    int ret;
    const uint32_t code[] = {
            /* Add two registers */
            0xd2800281, /* mov	x1, #0x14 */
            0xd28002c2, /* mov	x2, #0x16 */
            0x8b020023, /* add	x3, x1, x2 */

            /* Supervisor Call Exit */
            //0xd2800000, /* mov x0, #0x0 */
            //0x52800ba8, /* mov w8, #0x5d */
            //0xd4000001, /* svc #0x0 */

            /* Wait for Interrupt */
            0xd503207f, /* wfi */
    };
    uint8_t *mem;
    size_t mmap_size;
    struct kvm_run *run;

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
     * 0x0401F000 | Stack | grows downwards, so the SP is initially 0x0401FFFF
     * 0x04010000 | Heap  | grows upward
     * 0x04000000 | RAM   |
     * 0x00000000 | ROM   |
     *
     */
    check_kvm_extension(KVM_CAP_USER_MEMORY, "KVM_CAP_USER_MEMORY")
    mem = allocate_memory_to_vm(0x1000, 0x0);
    memcpy(mem, code, sizeof(code));
    mem = allocate_memory_to_vm(0x1000, 0x04000000);
    mem = allocate_memory_to_vm(0x1000, 0x04010000);
    mem = allocate_memory_to_vm(0x1000, 0x0401F000);
    check_kvm_extension(KVM_CAP_READONLY_MEM, "KVM_CAP_READONLY_MEM")
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

    /* Get register list. Somehow this is necessary */
    check_kvm_extension(KVM_CAP_ONE_REG, "KVM_CAP_ONE_REG")
    struct kvm_reg_list list;
    ret = ioctl(vcpufd, KVM_GET_REG_LIST, &list);
    if (ret < 0) {
        printf("System call 'KVM_GET_REG_LIST' failed: %s\n", strerror(errno));
        return ret;
    }

    check_kvm_extension(KVM_CAP_ONE_REG, "KVM_CAP_ONE_REG")
    /* Read register PSTATE (just for fun) */
    uint64_t val;
    get_register(kvm_regs.regs.pstate, &val, "PSTATE");

    /* Read register PC (just for fun) */
    get_register(kvm_regs.regs.pc, &val, "PC");

    /* Set register x2 to 42 and read it for verification (just for fun) */
    val = 42;
    set_register(kvm_regs.regs.regs[2], &val, "x2");
    val = 0;
    get_register(kvm_regs.regs.regs[2], &val, "x2");

    /* Repeatedly run code and handle VM exits. */
    printf("Running code\n");
    bool done = false;
    for (int i = 0; i < 10 && !done; i++) {
        printf("Loop %d\n", i);
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        printf("Loop %d\n", i);
        if (ret < 0) {
            printf("System call 'KVM_RUN' failed: %s\n", strerror(errno));
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

    /* Read register PC should not be 0 anymore */
    get_register(kvm_regs.regs.pc, &val, "PC");

    return 0;
}
