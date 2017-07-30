cmake_minimum_required(VERSION 3.2)

project(viewer)

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
    plambda.c
)

include_directories(imscript)

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

