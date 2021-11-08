kvm_test: hello_world.elf
	$(CXX) -o kvm_test kvm_test.cpp ./elf_loader/elf_loader.c -lstdc++ -lelf

hello_world.elf:
	$(MAKE) -C ./bare-metal-aarch64

clean:
	$(MAKE) clean -C ./bare-metal-aarch64
	rm -f kvm_test

