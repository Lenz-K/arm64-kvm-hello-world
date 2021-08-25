# Compile startup.s
as -o startup.o startup.s

# Compile hello_world.c
gcc -c -o hello_world.o hello_world.c

# Link the two object files to an elf file
ld -T link_script.ld startup.o hello_world.o -o hello_world.elf

# Create a text file with the objdump output of the elf file
objdump -D hello_world.elf > hello_world_objdump.txt

objcopy -R.comment -R.eh_frame -O binary hello_world.elf hello_world

hexdump -C hello_world
