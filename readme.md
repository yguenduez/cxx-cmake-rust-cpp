# CXX.rs with CMake Showcase

A Rust `rust_lib` crate exposes a stock quote API to a C++17 application through
[cxx.rs](https://cxx.rs/). The app uses SDL2, OpenGL, and Dear ImGui to display
quotes from [Finnhub](https://finnhub.io/).

## Requirements

- macOS or Debian/Ubuntu Linux
- A C++ compiler and [mise](https://mise.jdx.dev/)
- Homebrew (macOS) or `apt` (Linux)
- A free [Finnhub API key](https://finnhub.io/)

Internet access is needed to download Rust crates and Dear ImGui. On macOS the
minimum deployment target is `26.0`.

## Quick Start

```sh
git clone <repository-url>
cd cxx-cmake-rust-cpp
cp .env.example .env    # then set FINNHUB_API_KEY in .env
mise install            # Rust, CMake, Ninja
mise run setup          # install SDL2
mise run run            # build and launch
```

The first build is slower because Cargo and CMake compile the dependencies.

## Tasks

| Command | Description |
| --- | --- |
| `mise run build` | Build the Rust crate and the C++ app |
| `mise run run` | Build and launch the app |
| `mise run test` | Run the Rust test suite |
| `mise run test:rust:live` | Run tests including the live Finnhub call |
| `mise run lint` | Check Rust formatting and run Clippy |
| `mise run clean` | Remove build artifacts |

Enter a stock symbol such as `AAPL` and select **Get price**.

## Manual Build

Install the toolchain (Rust + Cargo, CMake 3.24+, Ninja, SDL2, OpenGL) and SDL2:

```sh
brew install sdl2            # macOS
sudo apt-get install libsdl2-dev   # Debian/Ubuntu
```

Export the API key and build:

```sh
export FINNHUB_API_KEY="<api-key>"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/cpp_app/cpp_app
```

CMake invokes Cargo, so a separate `cargo build` is not required.

## Project Structure

```text
.
|-- CMakeLists.txt
|-- cpp_app/
|   |-- include/stock/      # IStockApi interface, Quote, adapters
|   `-- src/                # SDL2/ImGui app, Rust and mock adapters
`-- rust_lib/
    |-- src/lib.rs          # Finnhub client and cxx.rs bridge
    |-- build.rs            # Generates and publishes bridge headers
    `-- rust_lib_cpp/       # CMake target exposed to the C++ app
```

The UI depends only on `stock::IStockApi`; `RustStockApi` implements it over the
cxx.rs bridge, and `MockStockApi` is available for offline use.

## How It Works

1. `rust_lib` fetches quotes from Finnhub; `cxx.rs` defines the FFI boundary.
2. `build.rs` publishes the generated headers into `rust_lib/rust_lib_cpp/include`.
3. CMake drives Cargo to build `librust_lib.a`, then builds and links `cpp_app`
   against the `rust_lib_cpp` target.

Generated headers and build output are recreated during the build and are not
committed.

## License

[MIT License](LICENSE)
