# About

All code is compiled and tested on an ARMv8-A processor running Ubuntu 21.04 using GNU Make 4.3.


# Contents

## 1. bare-metal-arm64-qemu

The folder `bare-metal-arm64-qemu` contains a bare metal ARM64 Assembly program that outputs "Hello World!\n".
It is an adaption of [this](https://github.com/freedomtan/aarch64-bare-metal-qemu) repository.   
`hello_world.elf` can be tested with QEMU. On a system with an ARM64 processor run:
```
qemu-system-aarch64 -M virt -cpu host -enable-kvm -nographic -kernel hello_world.elf
```
On another architecture it can be emulated. Remove `-enable-kvm` and replace `-cpu host` with `-cpu cortex-a57` for example.


## 2. bare-metal-arm64

The folder `bare-metal-arm64` contains an adaption of `bare-metal-arm64-qemu`, that is run in a VM by the KVM test program ([5. KVM Test Program](https://github.com/Lenz-K/kvm-test#5-kvm-test-program)).

### Sources
- https://developer.arm.com/documentation/102432/0100
- https://github.com/freedomtan/aarch64-bare-metal-qemu


## 3. KVM Test Program

The cpp-file `kvm_test.cpp` contains a program that sets up an ARM64 VM and executes the `bare-metal-arm64/hello-world.elf` program in the VM.
As a starting point, [this](https://lwn.net/Articles/658512/) KVM test program for x86 was used.
It is explained [here](https://lwn.net/Articles/658511/).
To change the code from x86 to ARM64 the [KVM API Documentation](https://www.kernel.org/doc/html/latest/virt/kvm/api.html) was used.

