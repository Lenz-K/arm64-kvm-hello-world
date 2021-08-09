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

string run_vm() {
    int kvm, vmfd, vcpufd, ret;
    const uint32_t code[] = {
            0xd2800281, /* mov	x1, #0x14 */
            0xd28002c2, /* mov	x2, #0x16 */
            0x8b020023, /* add	x3, x1, x2 */
            0xd2800000, /* mov	x0, #0x0 */
            0x52800ba8, /* mov	w8, #0x5d */
            0xd4000001, /* svc	#0x0 */
    };
    uint8_t *mem;
    size_t mmap_size;
    struct kvm_run *run;

    /* Get the KVM file descriptor */
    kvm = open("/dev/kvm", O_RDWR | O_CLOEXEC);
    if (kvm == -1) {
        string text = "Cannot open '/dev/kvm' - Cause: ";
        return text.append(strerror(errno));
    }

    /* Make sure we have the stable version of the API */
    ret = ioctl(kvm, KVM_GET_API_VERSION, NULL);
    if (ret == -1) {
        return "System call 'KVM_GET_API_VERSION' failed";
    }
    if (ret != 12) {
        string text = "expected KVM API Version 12 got: ";
        return text.append(to_string(ret));
    }

    /* Create a VM and receive the VM file descriptor */
    printf("Creating VM\n");
    vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long) 0);
    if (vmfd == -1) {
        return "System call 'KVM_CREATE_VM' failed";
    }

    /* Allocate one aligned page of guest memory to hold the code. */
    printf("Setting up memory\n");
    void *void_mem = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    mem = static_cast<uint8_t *>(void_mem);
    if (!mem)
        return "Error while allocating guest memory";
    memcpy(mem, code, sizeof(code));
    printf("Size of code: %ld B\n", sizeof(code));

    struct kvm_userspace_memory_region region = {
            .slot = 0,
            .guest_phys_addr = 0x0000,
            .memory_size = 0x1000,
            .userspace_addr = (uint64_t) mem,
    };
    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        return "System call 'KVM_SET_USER_MEMORY_REGION' failed";

    /* Create a virtual CPU and receive its file descriptor */
    printf("Creating VCPU\n");
    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long) 0);
    if (vcpufd == -1)
        return "System call 'KVM_CREATE_VCPU' failed";

    /* Get CPU information for VCPU init*/
    printf("Retrieving CPU information\n");
    struct kvm_vcpu_init preferred_target;
    ret = ioctl(vmfd, KVM_ARM_PREFERRED_TARGET, &preferred_target);
    if (ret == -1)
        return "System call 'KVM_ARM_PREFERRED_TARGET' failed";

    /* Initialize VCPU */
    printf("Initializing VCPU\n");
    ret = ioctl(vcpufd, KVM_ARM_VCPU_INIT, &preferred_target);
    if (ret == -1)
        return "System call 'KVM_ARM_VCPU_INIT' failed";

    /* Map the shared kvm_run structure and following data. */
    ret = ioctl(kvm, KVM_GET_VCPU_MMAP_SIZE, NULL);
    if (ret == -1)
        return "System call 'KVM_GET_VCPU_MMAP_SIZE' failed";
    mmap_size = ret;
    if (mmap_size < sizeof(*run))
        return "KVM_GET_VCPU_MMAP_SIZE unexpectedly small";
    void_mem = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);
    run = static_cast<kvm_run *>(void_mem);
    if (!run)
        return "Error while mmap vcpu";

    struct kvm_reg_list list;
    ret = ioctl(vcpufd, KVM_GET_REG_LIST, &list);
    if (ret == -1)
        return "System call 'KVM_GET_REG_LIST' failed";
    printf("Number of registers that can be set: %llu\n", list.n);

    /* Read register PSTATE */
    uint64_t val;
    uint64_t *val_ptr = &val;
    kvm_regs kvm_regs;
    struct kvm_one_reg reg {
        .id = kvm_regs.regs.pstate,
        .addr = (uintptr_t)val_ptr,
    };
    printf("Retrieving register PSTATE\n");
    ret = ioctl(vcpufd, KVM_GET_ONE_REG, &reg);
    if (ret == -1) {
        return "System call 'KVM_GET_ONE_REG' failed";
    }
    printf("PSTATE: %ld\n", val);

    /* Set register x2 to 42 and read it for verification */
    uint64_t val2 = 42;
    uint64_t *val2_ptr = &val2;
    struct kvm_one_reg reg2 {
        .id = kvm_regs.regs.regs[2],
        .addr = (uintptr_t)val2_ptr,
        };
    printf("Setting register x2 to 42\n");
    ret = ioctl(vcpufd, KVM_SET_ONE_REG, &reg2);
    if (ret == -1) {
        return "System call 'KVM_SET_ONE_REG' failed";
    }
    val2 = 0;
    printf("Retrieving register x2\n");
    ret = ioctl(vcpufd, KVM_GET_ONE_REG, &reg2);
    if (ret == -1) {
        return "System call 'KVM_GET_ONE_REG' failed";
    }
    printf("x2: %ld\n", val2);

    /* Repeatedly run code and handle VM exits. */
    printf("Running code\n");
    string res = "Run Result:\n";
    for (int i = 0; i < 10; i++) {
        printf("Loop %d\n", i);
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        printf("Loop %d\n", i);
        if (ret == -1)
            res += "System call 'KVM_RUN' failed";
        switch (run->exit_reason) {
            case KVM_EXIT_HLT:
                res += "KVM_EXIT_HLT\nSUCCESS!";
                return res;
            case KVM_EXIT_IO:
                res += "KVM_EXIT_IO";
                break;
            case KVM_EXIT_FAIL_ENTRY:
                res += "KVM_EXIT_FAIL_ENTRY";
                break;
            case KVM_EXIT_INTERNAL_ERROR:
                res += "KVM_EXIT_INTERNAL_ERROR";
                break;
            default:
                res += "KVM_EXIT other";
        }
        res += "\n";
    }

    return res;
}

int main() {
    string result = run_vm();
    printf("%s\n", result.c_str());
    return 0;
}
