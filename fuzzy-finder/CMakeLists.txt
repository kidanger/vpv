# Note: debug/release depending on the config
# "$<IF:$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>,release,debug>"
# Cannot be used in OUTPUT until CMake 3.20 (https://gitlab.kitware.com/cmake/cmake/-/issues/12877)
# For now, we always use release.

find_program(CARGO_EXE NAMES cargo cargo.exe)
if(NOT CARGO_EXE)
    message(FATAL_ERROR "cargo not found")
endif()

set(MURKY_RUST_LIB "${CMAKE_CURRENT_BINARY_DIR}/murky/release/${CMAKE_STATIC_LIBRARY_PREFIX}murky${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(MURKY_RUST_CXX "${CMAKE_CURRENT_BINARY_DIR}/murky/cxxbridge/murky/src/cxxbridge.rs.cc")
set(MURKY_RUST_H "${CMAKE_CURRENT_BINARY_DIR}/murky/cxxbridge/murky/src/cxxbridge.rs.h")

add_custom_command(
    OUTPUT "${MURKY_RUST_LIB}" "${MURKY_RUST_CXX}"
    COMMAND ${CARGO_EXE} build --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" --target-dir "${CMAKE_CURRENT_BINARY_DIR}/murky" --release
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/include"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MURKY_RUST_H}" "${CMAKE_CURRENT_BINARY_DIR}/include/murky.hpp"
    DEPENDS Cargo.toml src/lib.rs src/cxxbridge.rs
    COMMENT "Building murky"
)

add_library(murkycxx STATIC "${MURKY_RUST_CXX}")
target_link_libraries(murkycxx INTERFACE "${MURKY_RUST_LIB}")
target_include_directories(murkycxx INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")
if (WIN32)
    target_link_libraries(murkycxx PRIVATE userenv)
endif()

add_test(
    NAME murky-cargo-test
    COMMAND ${CARGO_EXE} test --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" --target-dir "${CMAKE_CURRENT_BINARY_DIR}/murky"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
