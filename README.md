# About

All code was compiled and tested on a Cortex-A72 (ARMv8-A) processor running
- Ubuntu 21.04 using GNU Make 4.3. (64 bit)
- Ubuntu 23.10 (64 bit)
- Raspberry Pi OS (October 10th 2023 Release) (64 bit)


# Contents

## 1. bare-metal-aarch64-qemu

The folder `bare-metal-aarch64-qemu` contains a bare metal AArch64 Assembly program that outputs "Hello World!\n".
It is an adaption of [this](https://github.com/freedomtan/aarch64-bare-metal-qemu) repository.   
`hello_world.elf` can be tested with QEMU.
On a system with an AArch64 processor and `qemu-system-aarch64` installed run:
```
qemu-system-aarch64 -M virt -cpu host -enable-kvm -nographic -kernel hello_world.elf
```
On another architecture it can be emulated. Remove `-enable-kvm` and replace `-cpu host` with `-cpu cortex-a72` for example.


## 2. bare-metal-aarch64

The folder `bare-metal-aarch64` contains an adaption of `bare-metal-aarch64-qemu`.
The purpose of it, is to be run in a VM by the KVM test program ([4. KVM Test Program](https://github.com/Lenz-K/arm64-kvm-hello-world#4-kvm-test-program)).
The build process ([Makefile](https://github.com/Lenz-K/arm64-kvm-hello-world/blob/main/bare-metal-aarch64/Makefile)) creates an ELF file.

### Sources
- https://developer.arm.com/documentation/102432/0100
- https://github.com/freedomtan/aarch64-bare-metal-qemu

## 3. elf-loader

The folder `elf-loader` contains a c-program that is used
(by [4. KVM Test Program](https://github.com/Lenz-K/arm64-kvm-hello-world#4-kvm-test-program))
to load the required sections of an ELF file into the memory of the VM.

### Sources
- Joseph Koshy, (2010, January, 12), "libelf by Example"

## 4. KVM Test Program

The cpp-file [kvm_test.cpp](https://github.com/Lenz-K/arm64-kvm-hello-world/blob/main/kvm_test.cpp) contains a program that sets up an AArch64 VM and executes the `bare-metal-aarch64/hello-world.elf` program in the VM.
As a starting point, [this](https://lwn.net/Articles/658512/) KVM test program for x86 was used.
It is explained [here](https://lwn.net/Articles/658511/).
To change the code from x86 to AArch64 the [KVM API Documentation](https://www.kernel.org/doc/html/latest/virt/kvm/api.html)
and the [QEMU source code](https://gitlab.com/qemu-project/qemu.git) were used.
