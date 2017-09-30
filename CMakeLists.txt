cmake_minimum_required(VERSION 2.8)

project(viewer)

option(USE_OCTAVE "compile with octave support" OFF)

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
    gmic/gmic.cpp
)

include_directories(imscript)
include_directories(gmic)

add_definitions(-Dgmic_build)
add_definitions(-Dcimg_use_zlib)
add_definitions(-Dcimg_use_vt100)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})

include_directories(imgui)
include_directories(imgui-sfml)
include_directories(iio)

set(STATIC_LIB true)
add_subdirectory(efsw)
include_directories(efsw/include)

add_executable(viewer ${SOURCES})
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

add_definitions(-Dcimg_use_fftw3)
target_link_libraries(viewer fftw3 fftw3_threads)

