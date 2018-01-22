cmake_minimum_required(VERSION 2.8)

project(vpv)

option(USE_OCTAVE "compile with octave support" OFF)
option(USE_FFTW "compile with fftw support (for gmic)" OFF)
option(USE_EXR "compile with OpenEXR support" OFF)
option(USE_GMIC "compile with GMIC support" OFF)

if(USE_OCTAVE)
set(OCTAVE_LIB_PATH "/usr/lib/octave/4.2.1/" CACHE PATH "path to octave libs")
set(OCTAVE_INCLUDE_PATH "/usr/include/octave-4.2.1/" CACHE PATH "path to octave libs")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -g")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -no-pie -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -no-pie -g")
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address -no-pie -g")

set(SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_demo.cpp
    external/imgui-sfml/imgui-SFML.cpp
    external/iio/iio.c
    src/main.cpp
    src/Window.cpp
    src/Sequence.cpp
    src/View.cpp
    src/Player.cpp
    src/Colormap.cpp
    src/Image.cpp
    src/Texture.cpp
    src/Shader.cpp
    src/shaders.cpp
    src/layout.cpp
    src/watcher.cpp
    src/wrapplambda.c
    src/SVG.cpp
    src/config.cpp
    src/editors.cpp
)

include_directories(external/others)

if(USE_GMIC)
    add_definitions(-DUSE_GMIC)
    set(SOURCES ${SOURCES} external/gmic/gmic.cpp)
    include_directories(external/gmic)

    add_definitions(-Dgmic_build)
    add_definitions(-Dcimg_use_zlib)
    add_definitions(-Dcimg_use_vt100)
endif()

include_directories(external/imscript)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/modules/")

include(GenerateLuaFiles)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

include_directories(external/imgui)
include_directories(external/imgui-sfml)
include_directories(external/iio)

set(STATIC_LIB ON CACHE BOOL "Build efsw as a static library" FORCE)
add_subdirectory(external/efsw)
include_directories(external/efsw/include)

include_directories(external/nanosvg/src)

include_directories(external/lua/src)

add_library(lua STATIC
    external/lua/src/lapi.c
    external/lua/src/lauxlib.c
    external/lua/src/lbaselib.c
    external/lua/src/lbitlib.c
    external/lua/src/lcode.c
    external/lua/src/lcorolib.c
    external/lua/src/lctype.c
    external/lua/src/ldblib.c
    external/lua/src/ldebug.c
    external/lua/src/ldo.c
    external/lua/src/ldump.c
    external/lua/src/lfunc.c
    external/lua/src/lgc.c
    external/lua/src/linit.c
    external/lua/src/liolib.c
    external/lua/src/llex.c
    external/lua/src/lmathlib.c
    external/lua/src/lmem.c
    external/lua/src/loadlib.c
    external/lua/src/lobject.c
    external/lua/src/lopcodes.c
    external/lua/src/loslib.c
    external/lua/src/lparser.c
    external/lua/src/lstate.c
    external/lua/src/lstring.c
    external/lua/src/lstrlib.c
    external/lua/src/ltable.c
    external/lua/src/ltablib.c
    external/lua/src/ltm.c
    external/lua/src/lua.c
    external/lua/src/luac.c
    external/lua/src/lundump.c
    external/lua/src/lutf8lib.c
    external/lua/src/lvm.c
    external/lua/src/lzio.c
)

add_executable(viewer ${SOURCES})
target_link_libraries(viewer lua)
target_link_libraries(viewer sfml-system)
target_link_libraries(viewer sfml-window)
target_link_libraries(viewer sfml-graphics)
target_link_libraries(viewer jpeg)
target_link_libraries(viewer png)
target_link_libraries(viewer tiff)
target_link_libraries(viewer ${OPENGL_LIBRARIES})
target_link_libraries(viewer pthread)
target_link_libraries(viewer efsw)
target_link_libraries(viewer z)

if(USE_OCTAVE)
    add_definitions(-DUSE_OCTAVE)
    include_directories(${OCTAVE_INCLUDE_PATH})
    include_directories(${OCTAVE_INCLUDE_PATH}/octave)
    target_link_libraries(viewer ${OCTAVE_LIB_PATH}/liboctave.so)
    target_link_libraries(viewer ${OCTAVE_LIB_PATH}/liboctinterp.so)
endif()

if(USE_FFTW)
    add_definitions(-Dcimg_use_fftw3)
    target_link_libraries(viewer fftw3 fftw3_threads)
endif()

if(USE_EXR)
	find_package(OpenEXR REQUIRED)
	add_definitions(-DI_CAN_HAS_LIBEXR) # for iio
	include_directories(${OPENEXR_INCLUDE_PATHS})
	target_link_libraries(viewer ${OPENEXR_LIBRARIES})
endif()

add_custom_command(
        TARGET viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_BINARY_DIR}/viewer
                ${CMAKE_CURRENT_BINARY_DIR}/vpv)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vpv DESTINATION bin)

if (CMAKE_GENERATOR STREQUAL "Ninja" AND
        ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9) OR
        (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5)))
        # Force colored warnings in Ninja's output, if the compiler has -fdiagnostics-color support.
        # Rationale in https://github.com/ninja-build/ninja/issues/814
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
    endif()

