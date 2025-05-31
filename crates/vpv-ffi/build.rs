fn main() {
    let sources = ["../fuzzy-finder/src/cxxbridge.rs"];
    cxx_build::bridges(sources).std("c++17").compile("vpv-ffi");

    for s in sources {
        println!("cargo:rerun-if-changed={s}");
    }
}
