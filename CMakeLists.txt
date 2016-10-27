cmake_minimum_required(VERSION 3.0)

project(viewer)

set(SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui-sfml/imgui-SFML.cpp
    main.cpp
)

include_directories(imgui)
include_directories(imgui-sfml)

add_executable(viewer ${SOURCES})
target_link_libraries(viewer sfml-system)
target_link_libraries(viewer sfml-window)
target_link_libraries(viewer sfml-graphics)
target_link_libraries(viewer GL)


