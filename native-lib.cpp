#include <jni.h>
#include <sys/ioctl.h>
#include <string>
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

string run_vm();

extern "C" JNIEXPORT jstring JNICALL
Java_edu_hm_karbaumer_lenz_kvm_1test_1app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {

    try {
        string res = run_vm();
        return env->NewStringUTF(res.c_str());
    }
    catch (const std::exception &e) {
        return env->NewStringUTF(e.what());
    }
}

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
    struct kvm_sregs sregs;
    size_t mmap_size;
    struct kvm_run *run;

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

    vmfd = ioctl(kvm, KVM_CREATE_VM, (unsigned long) 0);
    if (vmfd == -1) {
        return "System call 'KVM_CREATE_VM' failed";
    }

    /* Allocate one aligned page of guest memory to hold the code. */
    void *void_mem = mmap(NULL, 0x1000, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    mem = static_cast<uint8_t *>(void_mem);
    if (!mem)
        return "Error while allocating guest memory";
    memcpy(mem, code, sizeof(code));

    /* Map it to the second page frame (to avoid the real-mode IDT at 0). */
    struct kvm_userspace_memory_region region = {
            .slot = 0,
            .guest_phys_addr = 0x1000,
            .memory_size = 0x1000,
            .userspace_addr = (uint64_t) mem,
    };
    ret = ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &region);
    if (ret == -1)
        return "System call 'KVM_SET_USER_MEMORY_REGION' failed";

    vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, (unsigned long) 0);
    if (vcpufd == -1)
        return "System call 'KVM_CREATE_VCPU' failed";

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

    /* Initialize CS to point at 0, via a read-modify-write of sregs. */
    // ------------------------
    /*ret = ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    if (ret == -1)
        return "System call 'KVM_GET_SREGS' failed";
    sregs.cs.base = 0;
    sregs.cs.selector = 0;
    ret = ioctl(vcpufd, KVM_SET_SREGS, &sregs);
    if (ret == -1)
        return "System call 'KVM_SET_SREGS' failed";

    struct kvm_regs regs = {
            .rip = 0x1000,
            .rax = 2,
            .rbx = 2,
            .rflags = 0x2,
    };
    ret = ioctl(vcpufd, KVM_SET_REGS, &regs);
    if (ret == -1)
        return "System call 'KVM_SET_REGS' failed";*/

    /* Repeatedly run code and handle VM exits. */
    string res = "Run Result:\n";
    for (int i = 0; i < 10; i++) {
        ret = ioctl(vcpufd, KVM_RUN, NULL);
        if (ret == -1)
            res += "System call 'KVM_RUN' failed";
        switch (run->exit_reason) {
            case KVM_EXIT_HLT:
                res += "KVM_EXIT_HLT";
            case KVM_EXIT_IO:
                res += "KVM_EXIT_IO";
                break;
            case KVM_EXIT_FAIL_ENTRY:
                res += "KVM_EXIT_FAIL_ENTRY";
            case KVM_EXIT_INTERNAL_ERROR:
                res += "KVM_EXIT_INTERNAL_ERROR";
            default:
                res += "KVM_EXIT other";
        }
        res += "\n";
    }

    return res;
}