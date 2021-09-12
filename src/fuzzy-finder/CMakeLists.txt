# Note: debug/release depending on the config
# "$<IF:$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>,$<CONFIG:RelWithDebInfo>>,release,debug>"
# Cannot be used in OUTPUT until CMake 3.20 (https://gitlab.kitware.com/cmake/cmake/-/issues/12877)
# For now, we always use release.

set(FUZZYFINDER_RUST_LIB "${CMAKE_CURRENT_BINARY_DIR}/fuzzy-finder/release/${CMAKE_STATIC_LIBRARY_PREFIX}fuzzy_finder${CMAKE_STATIC_LIBRARY_SUFFIX}")
set(FUZZYFINDER_RUST_CXX "${CMAKE_CURRENT_BINARY_DIR}/fuzzy-finder/cxxbridge/fuzzy-finder/src/cxxbridge.rs.cc")
set(FUZZYFINDER_RUST_H "${CMAKE_CURRENT_BINARY_DIR}/fuzzy-finder/cxxbridge/fuzzy-finder/src/cxxbridge.rs.h")

add_custom_command(
    OUTPUT "${FUZZYFINDER_RUST_LIB}" "${FUZZYFINDER_RUST_CXX}"
    COMMAND ${CARGO_EXE} build --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" --target-dir "${CMAKE_CURRENT_BINARY_DIR}/fuzzy-finder" --release
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/include"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FUZZYFINDER_RUST_H}" "${CMAKE_CURRENT_BINARY_DIR}/include/fuzzy-finder.hpp"
    DEPENDS Cargo.toml build.rs src/lib.rs src/cxxbridge.rs
    COMMENT "Building fuzzy-finder"
)

add_library(fuzzyfindercxx STATIC "${FUZZYFINDER_RUST_CXX}")
target_link_libraries(fuzzyfindercxx INTERFACE "${FUZZYFINDER_RUST_LIB}")
target_include_directories(fuzzyfindercxx INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")
if (WIN32)
    target_link_libraries(fuzzyfindercxx PRIVATE userenv)
endif()

add_test(
    NAME fuzzy-finder-cargo-test
    COMMAND ${CARGO_EXE} test --manifest-path "${CMAKE_CURRENT_SOURCE_DIR}/Cargo.toml" --target-dir "${CMAKE_CURRENT_BINARY_DIR}/fuzzy-finder"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
