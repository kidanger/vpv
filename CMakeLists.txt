cmake_minimum_required(VERSION 2.8)

project(vpv)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/modules/")


#################
##
##  OPTIONS
##
#################

option(USE_EXR "compile with OpenEXR support (for iio)" OFF)
option(USE_GMIC "compile with GMIC support" OFF)
option(USE_FFTW "compile with fftw support (for gmic)" OFF)
option(USE_OCTAVE "compile with octave support" OFF)
option(USE_SDL "compile with SDL backend instead of SFMl" ON)


#################
##
##  FLAGS
##
#################

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if (CMAKE_VERSION VERSION_LESS "3.1")
    set(CMAKE_C_FLAGS "-std=c11 ${CMAKE_C_FLAGS}")
else()
    set(CMAKE_C_STANDARD 11)
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -g -O3")

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -no-pie -g")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -no-pie -g")
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address -no-pie -g")

include_directories(external/imgui)
include_directories(external/imgui/misc/fonts)
include_directories(external/imscript)
include_directories(external/others)
include_directories(external/nanosvg/src)
include_directories(external/lua/src)
include_directories(external/kaguya/include)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})


#################
##
##  GMIC
##
#################

if(USE_GMIC)
    add_definitions(-DUSE_GMIC)
    set(SOURCES ${SOURCES} external/gmic/gmic.cpp)
    include_directories(external/gmic)

    add_definitions(-Dgmic_build)
    add_definitions(-Dcimg_use_zlib)
    add_definitions(-Dcimg_use_vt100)
endif()


#################
##
##  FFTW
##
#################

if(USE_FFTW)
    add_definitions(-Dcimg_use_fftw3)
    set(LIBS ${LIBS} fftw3 fftw3_threads)
endif()


#################
##
##  OCTAVE
##
#################

if(USE_OCTAVE)
    add_definitions(-DUSE_OCTAVE)

    # TODO: this should be changed
    set(OCTAVE_LIB_PATH "/usr/lib/octave/4.4.0" CACHE PATH "path to octave libs")
    set(OCTAVE_INCLUDE_PATH "/usr/include/octave-4.4.0" CACHE PATH "path to octave libs")

    include_directories(${OCTAVE_INCLUDE_PATH})
    include_directories(${OCTAVE_INCLUDE_PATH}/octave)

    set(LIBS ${LIBS} ${OCTAVE_LIB_PATH}/liboctave.so ${OCTAVE_LIB_PATH}/liboctinterp.so)
endif()


#################
##
##  IIO
##
#################

add_library(iio external/iio/iio.c)
include_directories(external/iio)
target_link_libraries(iio jpeg png tiff)

if(USE_EXR)
	find_package(OpenEXR REQUIRED)
	add_definitions(-DI_CAN_HAS_LIBEXR) # for iio
	include_directories(${OPENEXR_INCLUDE_PATHS})
	target_link_libraries(iio ${OPENEXR_LIBRARIES})
endif()

set(LIBS ${LIBS} iio)


#################
##
##  EFSW
##
#################

set(STATIC_LIB ON CACHE BOOL "Build efsw as a static library" FORCE)
add_subdirectory(external/efsw)
include_directories(external/efsw/include)


#################
##
##  LUA
##
#################

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

#################
##
##  SDL
##
#################

if(USE_SDL)
    add_definitions(-DSDL)

    set(SOURCES ${SOURCES}
        external/imgui/examples/sdl_opengl2_example/imgui_impl_sdl_gl2.cpp
        external/imgui/examples/libs/gl3w/GL/gl3w.c
    )

    include_directories(external/imgui/examples/libs/gl3w)
    include_directories(external/imgui/examples/sdl_opengl2_example)

    set(LIBS ${LIBS} SDL2)
endif()


#################
##
##  SFML (deprecated)
##
#################

if(NOT USE_SDL)
    set(SOURCES ${SOURCES}
        external/imgui-sfml/imgui-SFML.cpp
    )

    find_package(GLEW REQUIRED)

    include_directories(${GLEW_INCLUDE_DIR})
    include_directories(external/imgui-sfml)

    set(LIBS ${LIBS} ${GLEW_LIBRARIES} sfml-system sfml-window sfml-graphics)
endif()


#################
##
##  VPV
##
#################
set(SOURCES ${SOURCES}
    src/main.cpp
    src/Window.cpp
    src/Sequence.cpp
    src/View.cpp
    src/Player.cpp
    src/Colormap.cpp
    src/Image.cpp
    src/Texture.cpp
    src/DisplayArea.cpp
    src/Shader.cpp
    src/shaders.cpp
    src/layout.cpp
    src/watcher.cpp
    src/wrapplambda.c
    src/SVG.cpp
    src/config.cpp
    src/editors.cpp
    src/events.cpp
    src/imgui_custom.cpp
    src/ImageCache.cpp
    src/ImageCollection.cpp
    src/ImageProvider.cpp
    src/LoadingThread.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_demo.cpp
)

include(GenerateLuaFiles)

add_executable(vpv ${SOURCES})

set(LIBS ${LIBS}
    lua
    ${OPENGL_LIBRARIES}
    pthread
    efsw
    z
    dl
)
target_link_libraries(vpv ${LIBS})


#################
##
##  MISC
##
#################

add_custom_command(
        TARGET vpv POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_BINARY_DIR}/vpv
                ${CMAKE_CURRENT_BINARY_DIR}/viewer)

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vpv DESTINATION bin)

if (CMAKE_GENERATOR STREQUAL "Ninja" AND
    ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9) OR
    (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5)))
    # Force colored warnings in Ninja's output, if the compiler has -fdiagnostics-color support.
    # Rationale in https://github.com/ninja-build/ninja/issues/814
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

