kvm_test: bare-metal-arm64
	gcc -o kvm_test kvm_test.cpp -lstdc++

bare-metal-arm64:
	$(MAKE) -C ./bare-metal-arm64

