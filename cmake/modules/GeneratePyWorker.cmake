# Embed src/pyworker.py into the binary as a C string.
#
# The worker is passed to the interpreter with `python3 -c <source>`, which
# avoids having to locate a data file at runtime (and therefore avoids caring
# about install prefixes, AppImage mounts and macOS bundles).

set(PYWORKER_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/pyworker.py)
set(PYWORKER_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pyworker_source.cpp)

file(READ ${PYWORKER_INPUT} PYWORKER_CONTENT)

# A raw string literal needs no escaping, only a delimiter that cannot appear in
# the payload. Fail loudly rather than silently generating broken C++.
if(PYWORKER_CONTENT MATCHES "\\)PYWORKER\"")
    message(FATAL_ERROR "src/pyworker.py contains the raw string delimiter )PYWORKER\"")
endif()

file(WRITE ${PYWORKER_OUTPUT} "// Generated from src/pyworker.py by GeneratePyWorker.cmake. Do not edit.

extern const char* const PYWORKER_SOURCE;

const char* const PYWORKER_SOURCE = R\"PYWORKER(${PYWORKER_CONTENT})PYWORKER\";
")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PYWORKER_INPUT})

list(APPEND SOURCES ${PYWORKER_OUTPUT})
