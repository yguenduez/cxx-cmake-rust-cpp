# CXX.rs with CMake Showcase

## About

This is an examplary usage of FFI, where a rust library is used in C++ context.
In this example we will use cxx.rs, which will build our FFI glue and directly wraps it with a C++ header and `.cpp` file.
We will use CMake to link the created static library from Rust against the CPP App, that will consume it.

