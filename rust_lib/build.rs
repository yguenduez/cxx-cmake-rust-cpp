// cxx-build writes the generated headers to `$CARGO_TARGET_DIR/cxxbridge`,
// which the `rust_lib_cpp` CMake target adds to its include path. It also
// emits `rerun-if-changed` for the bridge file.
fn main() {
    cxx_build::bridge("src/lib.rs")
        .std("c++17")
        .compile("rust_lib");
}
