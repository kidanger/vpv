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

set(SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui-sfml/imgui-SFML.cpp
    iio/iio.c
    main.cpp
    Window.cpp
    Sequence.cpp
    View.cpp
    Player.cpp
    Colormap.cpp
    Image.cpp
    Texture.cpp
    Shader.cpp
    shaders.cpp
    layout.cpp
    watcher.cpp
    wrapplambda.c
    SVG.cpp
    config.cpp
)

if(USE_GMIC)
    add_definitions(-DUSE_GMIC)
    set(SOURCES ${SOURCES} gmic/gmic.cpp)
    include_directories(gmic)

    add_definitions(-Dgmic_build)
    add_definitions(-Dcimg_use_zlib)
    add_definitions(-Dcimg_use_vt100)
endif()

include_directories(imscript)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/modules/")

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

include_directories(imgui)
include_directories(imgui-sfml)
include_directories(iio)

set(STATIC_LIB ON CACHE BOOL "Build efsw as a static library" FORCE)
add_subdirectory(efsw)
include_directories(efsw/include)

include_directories(nanosvg/src)

include_directories(lua/src)

add_library(lua STATIC
    lua/src/lapi.c
    lua/src/lauxlib.c
    lua/src/lbaselib.c
    lua/src/lbitlib.c
    lua/src/lcode.c
    lua/src/lcorolib.c
    lua/src/lctype.c
    lua/src/ldblib.c
    lua/src/ldebug.c
    lua/src/ldo.c
    lua/src/ldump.c
    lua/src/lfunc.c
    lua/src/lgc.c
    lua/src/linit.c
    lua/src/liolib.c
    lua/src/llex.c
    lua/src/lmathlib.c
    lua/src/lmem.c
    lua/src/loadlib.c
    lua/src/lobject.c
    lua/src/lopcodes.c
    lua/src/loslib.c
    lua/src/lparser.c
    lua/src/lstate.c
    lua/src/lstring.c
    lua/src/lstrlib.c
    lua/src/ltable.c
    lua/src/ltablib.c
    lua/src/ltm.c
    lua/src/lua.c
    lua/src/luac.c
    lua/src/lundump.c
    lua/src/lutf8lib.c
    lua/src/lvm.c
    lua/src/lzio.c
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


