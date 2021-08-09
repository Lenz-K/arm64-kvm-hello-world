# cmake_minimum_required(VERSION <specify CMake version here>)
project(kvm_test)

set(CMAKE_CXX_STANDARD 14)

add_executable(kvm_test
        kvm-test.cpp)
