all: rom.dat ram.dat

hello_world.elf: link_script.ld startup.o hello_world.o
	# Link the two object files to an elf file
	$(LD) -T link_script.ld startup.o hello_world.o -o hello_world.elf

	# Create a text file with the objdump output of the elf file
	objdump -D hello_world.elf > hello_world_objdump.txt

startup.o: startup.s
	$(AS) -o startup.o startup.s

hello_world.o: hello_world.c
	$(CC) -c -o hello_world.o hello_world.c

rom.dat: hello_world.elf
	# Create binary file with ROM contents
	objcopy -j.startup -O binary hello_world.elf rom.dat
	echo ROM:
	hexdump rom.dat

ram.dat: hello_world.elf
	# Create binary file with RAM contents
	objcopy -j.text -j.rodata -j.eh_frame -O binary hello_world.elf ram.dat
	echo RAM:
	hexdump ram.dat

clean:
	rm -f hello_world.o startup.o hello_world.elf rom.dat ram.dat hello_world_objdump.txt
