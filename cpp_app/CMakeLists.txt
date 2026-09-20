# Dear ImGui: no upstream CMake package, so build it from source. SDL2 + OpenGL3
# is used because that combination works on Windows, Linux and macOS.
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Platform dependencies.
find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
target_link_libraries(imgui PUBLIC SDL2::SDL2 OpenGL::GL)

# The application.
add_executable(cpp_app
    src/main.cpp
    src/mock_stock_api.cpp
)
target_include_directories(cpp_app PRIVATE include)
target_compile_features(cpp_app PRIVATE cxx_std_17)
target_link_libraries(cpp_app PRIVATE imgui)
