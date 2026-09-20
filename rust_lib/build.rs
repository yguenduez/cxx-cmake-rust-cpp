use std::env;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

fn main() {
    cxx_build::bridge("src/lib.rs")
        .std("c++17")
        .compile("rust_lib");

    println!("cargo:rerun-if-changed=src/lib.rs");

    if let Err(err) = publish_headers() {
        panic!("failed to publish cxx headers into rust_lib_cpp: {err}");
    }
}

/// Copy the cxx-generated headers out of `OUT_DIR` into `rust_lib_cpp/include`
/// so the C++ side can `#include "rust_lib/src/lib.rs.h"`.
fn publish_headers() -> io::Result<()> {
    let out_dir = PathBuf::from(env::var_os("OUT_DIR").expect("OUT_DIR is set by cargo"));
    let generated = out_dir.join("cxxbridge").join("include");
    let destination = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("rust_lib_cpp")
        .join("include");

    copy_dir(&generated, &destination)
}

fn copy_dir(from: &Path, to: &Path) -> io::Result<()> {
    fs::create_dir_all(to)?;
    for entry in fs::read_dir(from)? {
        let entry = entry?;
        let target = to.join(entry.file_name());
        if entry.file_type()?.is_dir() {
            copy_dir(&entry.path(), &target)?;
        } else {
            fs::copy(entry.path(), &target)?;
        }
    }
    Ok(())
}
