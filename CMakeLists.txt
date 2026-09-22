cmake_minimum_required(VERSION 3.24)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND
   NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "26.0" CACHE STRING
        "Minimum macOS version supported by the build")
endif()

project(cxx_cmake_rust_cpp LANGUAGES CXX)

add_subdirectory(rust_lib/rust_lib_cpp)
add_subdirectory(cpp_app)
