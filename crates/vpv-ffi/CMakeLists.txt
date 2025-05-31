# Note: debug/release depending on the config
# "$<IF:$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>,release,debug>"
# Cannot be used in OUTPUT until CMake 3.20 (https://gitlab.kitware.com/cmake/cmake/-/issues/12877)
# For now, we always use release.

set(VPVFFI_RUST_LIB "${CMAKE_CURRENT_BINARY_DIR}/release/${CMAKE_STATIC_LIBRARY_PREFIX}vpv_ffi${CMAKE_STATIC_LIBRARY_SUFFIX}")

set(VPVFFI_CRATES
    "fuzzy-finder"
)

set(VPVFFI_RUST_CXX)
set(COPY_COMMANDS)
foreach(CRATE ${VPVFFI_CRATES})
    list(APPEND VPVFFI_RUST_CXX "${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/vpv-ffi/${CRATE}/src/cxxbridge.rs.cc")
    list(APPEND COPY_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/cxxbridge/vpv-ffi/${CRATE}/src/cxxbridge.rs.h"
        "${CMAKE_CURRENT_BINARY_DIR}/include/vpv-ffi/${CRATE}.hpp"
    )
endforeach()

add_custom_command(
    OUTPUT "${VPVFFI_RUST_LIB}" ${VPVFFI_RUST_CXX}
    COMMAND ${CMAKE_COMMAND} -E env --unset=CARGO_TARGET_DIR LIBVPVFFI_BUILD_DIR="${CMAKE_CURRENT_BINARY_DIR}" ${CARGO_EXE} build --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" --target-dir "${CMAKE_CURRENT_BINARY_DIR}/" --release
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/include/vpv-ffi/"
    ${COPY_COMMANDS}
    DEPENDS Cargo.toml build.rs
    COMMENT "Building vpv-ffi"
)

add_library(vpv-ffi STATIC ${VPVFFI_RUST_CXX})
target_link_libraries(vpv-ffi INTERFACE "${VPVFFI_RUST_LIB}")
target_include_directories(vpv-ffi INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")

