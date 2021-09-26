kvm_test: memory.h
	$(CXX) -o kvm_test kvm_test.cpp -lstdc++

memory.h:
	$(MAKE) -C ./bare-metal-aarch64

clean:
	$(MAKE) clean -C ./bare-metal-aarch64
	rm -f kvm_test

