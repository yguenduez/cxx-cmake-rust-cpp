find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(RUST_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../target/release/librust_lib.a)

# This is basically cargo build --release with CMake
add_custom_target(rust_lib_build
    COMMAND ${CARGO_EXECUTABLE} build --release
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    BYPRODUCTS ${RUST_LIB}
    VERBATIM
)

add_library(rust_lib_cpp INTERFACE)
add_dependencies(rust_lib_cpp rust_lib_build)
target_include_directories(rust_lib_cpp INTERFACE include)
target_link_libraries(rust_lib_cpp INTERFACE ${RUST_LIB})

if(APPLE)
    target_link_libraries(rust_lib_cpp INTERFACE iconv)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(rust_lib_cpp INTERFACE dl pthread m)
else()
    message(FATAL_ERROR "rust_lib_cpp only supports macOS and Linux")
endif()
