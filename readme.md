# CXX.rs with CMake Showcase

This project demonstrates how to use a Rust library from a C++ application through
[cxx.rs](https://cxx.rs/).

Rust builds a static library and generates the C++ bridge headers. CMake then builds
and links a C++17 application that uses SDL2, OpenGL, and Dear ImGui to display stock
quotes retrieved from [Finnhub](https://finnhub.io/).

## Requirements

Supported platforms:

- macOS
- Debian or Ubuntu Linux

You will need:

- Git
- A C++ compiler
- [mise](https://mise.jdx.dev/)
- Homebrew on macOS, or `apt` on Debian/Ubuntu
- A free [Finnhub API key](https://finnhub.io/)

Internet access is also required to download Rust dependencies and Dear ImGui.

## Quick Start

Clone the repository and enter the project directory:

```sh
git clone <repository-url>
cd cxx-cmake-rust-cpp
```

Create the local environment file:

```sh
cp .env.example .env
```

Open `.env` and replace `<api-key>` with your Finnhub API key:

```dotenv
FINNHUB_API_KEY=<api-key>
```

Install the required Rust, CMake, and Ninja versions:

```sh
mise install
```

Install SDL2:

```sh
mise run setup
```

Build and run the application:

```sh
mise run run
```

The first build may take a little longer because Cargo and CMake download and compile
the project dependencies.

Enter a stock symbol such as `AAPL` in the application and select **Get price**.

## Build and Run Separately

To build without starting the application:

```sh
mise run build
```

Run the compiled application:

```sh
./build/cpp_app/cpp_app
```

## Manual Build

If you do not want to use the mise tasks, install these dependencies yourself:

- Rust with Cargo
- CMake 3.24 or newer
- Ninja
- SDL2 development files
- An OpenGL development environment

On macOS, install SDL2 with Homebrew:

```sh
brew install sdl2
```

On Debian or Ubuntu:

```sh
sudo apt-get update
sudo apt-get install -y libsdl2-dev
```

Export your Finnhub API key:

```sh
export FINNHUB_API_KEY="<api-key>"
```

Configure and build the project:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run the application:

```sh
./build/cpp_app/cpp_app
```

CMake invokes Cargo automatically, so a separate `cargo build` is not required.

## Development Commands

Run the Rust unit tests:

```sh
mise run test
```

Run the ignored test that makes a live Finnhub request:

```sh
mise run test:rust:live
```

Check Rust formatting and run Clippy:

```sh
mise run lint
```

Remove generated build artifacts:

```sh
mise run clean
```

## Project Structure

```text
.
|-- CMakeLists.txt
|-- cpp_app/
|   |-- include/             # C++ stock API interfaces
|   `-- src/                 # SDL2/ImGui application and Rust adapter
`-- rust_lib/
    |-- src/lib.rs           # Rust API client and cxx.rs bridge
    |-- build.rs             # Generates and publishes bridge headers
    `-- rust_lib_cpp/        # CMake target exposed to the C++ application
```

## How It Works

1. `rust_lib` retrieves stock quotes from Finnhub.
2. `cxx.rs` generates the bindings used at the Rust/C++ boundary.
3. `build.rs` publishes the generated headers under `rust_lib/rust_lib_cpp/include`.
4. CMake invokes Cargo to build `librust_lib.a`.
5. The `rust_lib_cpp` CMake target exposes the Rust library and generated headers.
6. `cpp_app` links that target and calls the Rust implementation through a C++ interface.

The generated bridge headers and build output are not committed; they are recreated
during the build.

## License

This project is licensed under the [MIT License](LICENSE).
