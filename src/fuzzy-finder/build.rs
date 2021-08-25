fn main() {
    let _build = cxx_build::bridge("src/cxxbridge.rs");
    println!("cargo:rerun-if-changed=src/cxxbridge.rs");
}
