# A Hello World Program in ARM64 Assembly

The folder `arm-assembly-hello-world` contains an ARM64 Assembly program that outputs "Hello World!\n". It was assembled to an object file with `as -o hello-world.o hello-world.s` and then linked with `ld -s -o hello-world hello-world.o`.
With `objdump -D hello-world > hello-world-objdump.txt` a textfile was created, that contains the translation between ARM64 Assembly instructions and machine code instructions.


# A Minimalistic ARM64 Assembly Program

The folder `arm-assembly-calc` contains an ARM64 Assembly program that sets two registers to the values 20 and 22 and then adds the two registers.


# KVM Test Program

The cpp-file `kvm-test.cpp` contains a program that shall setup an ARM64 VM and execute the arm-assembly-calc program in the VM.
