cmake_minimum_required(VERSION 3.24)

project(cxx_cmake_rust_cpp LANGUAGES CXX)

add_subdirectory(rust_lib/rust_lib_cpp)
add_subdirectory(cpp_app)
