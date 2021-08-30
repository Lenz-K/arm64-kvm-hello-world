hello_world.elf: link_script.ld startup.o hello_world.o
	# Link the two object files to an elf file
	ld -T link_script.ld startup.o hello_world.o -o hello_world.elf

startup.o: startup.s
	as -o startup.o startup.s

hello_world.o: hello_world.c
	gcc -c -o hello_world.o hello_world.c
