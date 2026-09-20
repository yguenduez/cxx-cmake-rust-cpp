# C++ view of the cxx.rs bindings generated from rust_lib's bridge.
#
# cxx-build already compiles the bridge glue into `librust_lib.a`, so this
# target only exposes the generated headers and the Rust static library (plus
# the native libraries a C++ linker needs for a Rust staticlib).

cmake_minimum_required(VERSION 3.24)

find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(RUST_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/../Cargo.toml)
set(RUST_TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../target)

if(WIN32)
    set(RUST_STATIC_LIB ${RUST_TARGET_DIR}/release/rust_lib.lib)
else()
    set(RUST_STATIC_LIB ${RUST_TARGET_DIR}/release/librust_lib.a)
endif()

# Build once at configure time. This also generates the cxx headers under
# OUT_DIR, which lets us discover their location and ask rustc which native
# libraries a Rust staticlib needs on this platform.
execute_process(
    COMMAND ${CARGO_EXECUTABLE} rustc --release --manifest-path ${RUST_MANIFEST}
            -- --print native-static-libs
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_QUIET
    ERROR_VARIABLE _rustc_stderr
    RESULT_VARIABLE _rustc_result
)
if(_rustc_result EQUAL 0 AND _rustc_stderr MATCHES "native-static-libs:([^\n\r]*)")
    separate_arguments(RUST_NATIVE_LIBS UNIX_COMMAND "${CMAKE_MATCH_1}")
else()
    message(WARNING "Could not determine Rust native-static-libs; using defaults")
    if(APPLE)
        set(RUST_NATIVE_LIBS "-framework" "Security" "-liconv" "-lSystem" "-lresolv")
    elseif(WIN32)
        set(RUST_NATIVE_LIBS ws2_32 userenv advapi32 bcrypt ntdll kernel32)
    else()
        set(RUST_NATIVE_LIBS dl pthread m gcc_s c)
    endif()
endif()

# Locate the cxx-generated header directory produced by build.rs.
file(GLOB RUST_HEADER_SOURCES LIST_DIRECTORIES true
    ${RUST_TARGET_DIR}/release/build/rust_lib-*/out/cxxbridge/include
)
list(SORT RUST_HEADER_SOURCES)
list(LENGTH RUST_HEADER_SOURCES _header_source_count)
if(_header_source_count GREATER 0)
    math(EXPR _header_source_last "${_header_source_count} - 1")
    list(GET RUST_HEADER_SOURCES ${_header_source_last} RUST_HEADER_SOURCE)
else()
    set(RUST_HEADER_SOURCE ${RUST_TARGET_DIR}/release/build/rust_lib-unknown/out/cxxbridge/include)
endif()
set(RUST_HEADER_DEST ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Phony target: runs on every build so the headers in include/ are refreshed
# even if the build tree was cleaned while cargo still considers build.rs
# up to date (in which case build.rs would otherwise not run again).
add_custom_target(rust_lib_build
    COMMAND ${CARGO_EXECUTABLE} build --release --manifest-path ${RUST_MANIFEST}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${RUST_HEADER_SOURCE} ${RUST_HEADER_DEST}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMMENT "cargo build --release (rust_lib) and publish cxx headers"
    VERBATIM
)

add_library(rust_lib_static STATIC IMPORTED GLOBAL)
set_target_properties(rust_lib_static PROPERTIES
    IMPORTED_LOCATION ${RUST_STATIC_LIB}
)

add_library(rust_lib_cpp INTERFACE)
target_include_directories(rust_lib_cpp INTERFACE ${RUST_HEADER_DEST})
target_link_libraries(rust_lib_cpp INTERFACE rust_lib_static ${RUST_NATIVE_LIBS})
