kvm_test: bare-metal-arm64
	$(CXX) -o kvm_test kvm_test.cpp -lstdc++

bare-metal-arm64:
	$(MAKE) -C ./bare-metal-arm64

clean:
	rm -f kvm_test

