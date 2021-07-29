# A Hello World Program in ARM64 Assembly

The folder `arm-assembly-hello-world` contains an ARM64 Assembly program that outputs "Hello World!\n". It was assembled to an object file with `as -o hello-world.o hello-world.s` and then linked with `ld -s -o hello-world hello-world.o`.
With `objdump -D hello-world > hello-world-objdump.txt` a textfile was created, that contains the translation between ARM64 Assembly instructions and machine code instructions.
