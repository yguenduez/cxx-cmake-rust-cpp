find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(CARGO_TARGET_DIR ${CMAKE_CURRENT_BINARY_DIR}/cargo)

# Debug builds use Cargo's dev profile (output in `debug/`), all others release.
set(CARGO_PROFILE $<IF:$<CONFIG:Debug>,dev,release>)
set(CARGO_PROFILE_DIR $<IF:$<CONFIG:Debug>,debug,release>)
set(RUST_LIB ${CARGO_TARGET_DIR}/${CARGO_PROFILE_DIR}/librust_lib.a)

add_custom_target(rust_lib_build
    COMMAND ${CMAKE_COMMAND} -E env
        CARGO_TARGET_DIR=${CARGO_TARGET_DIR}
        MACOSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
        ${CARGO_EXECUTABLE} build --locked --profile ${CARGO_PROFILE}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    BYPRODUCTS ${RUST_LIB}
    VERBATIM
)

add_library(rust_lib_cpp INTERFACE)
add_dependencies(rust_lib_cpp rust_lib_build)
target_include_directories(rust_lib_cpp INTERFACE ${CARGO_TARGET_DIR}/cxxbridge)
target_link_libraries(rust_lib_cpp INTERFACE ${RUST_LIB})

if(APPLE)
    target_link_libraries(rust_lib_cpp INTERFACE iconv)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(rust_lib_cpp INTERFACE dl pthread m)
else()
    message(FATAL_ERROR "rust_lib_cpp only supports macOS and Linux")
endif()
