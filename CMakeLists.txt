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
option(USE_SDL "compile with SDL backend instead of SFML" ON)
option(USE_GL3 "compile with OpenGL3 backend instead of OpenGL2" ON)
option(USE_LIBRAW "compile with LibRAW support" OFF)
option(USE_GDAL "compile with GDAL support" OFF)

if(MSYS)
	set(WINDOWS 1)
	add_definitions(-DWINDOWS)
endif()
add_definitions(-DHAS_GLOB)


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


#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
#set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -g -O3")

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

if(NOT WINDOWS)
   if(POLICY CMP0072)
      cmake_policy(SET CMP0072 NEW)
   endif()
   find_package(OpenGL REQUIRED)
   include_directories(${OPENGL_INCLUDE_DIR})
else()
   set(OPENGL_LIBRARIES opengl32)
endif()


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

# supported versions:
#  - octave 4.2.2 (ubuntu 18.04)
#  - octave 4.4.1
#  - octave 5.2.0
if(USE_OCTAVE)
    add_definitions(-DUSE_OCTAVE)

    find_package(Octave)
    message("The previous message is not important.")

    include_directories(${OCTAVE_INCLUDE_DIRS}/..)
    if(OCTAVE_VERSION_STRING STREQUAL "4.2.2")
        include_directories(${OCTAVE_INCLUDE_DIRS})
    endif()

    set(LIBS ${LIBS} ${OCTAVE_OCTINTERP_LIBRARY} ${OCTAVE_OCTAVE_LIBRARY})
endif()


#################
##
##  IIO
##
#################

add_library(iio STATIC external/iio/iio.c external/iio/npy.c)
include_directories(external/iio)

find_package(PNG REQUIRED)
include_directories(${PNG_INCLUDE_DIRS})
target_link_libraries(iio ${PNG_LIBRARIES})

find_package(TIFF REQUIRED)
include_directories(${TIFF_INCLUDE_DIRS})
target_link_libraries(iio ${TIFF_LIBRARIES})

find_package(JPEG REQUIRED)
include_directories(${JPEG_INCLUDE_DIRS})
target_link_libraries(iio ${JPEG_LIBRARIES})

if(USE_EXR)
	find_package(OpenEXR REQUIRED)
	add_definitions(-DI_CAN_HAS_LIBEXR) # for iio
	include_directories(${OPENEXR_INCLUDE_PATHS})
	target_link_libraries(iio ${OPENEXR_LIBRARIES})
endif()

set(LIBS ${LIBS} iio)


#################
##
##  LIBRAW
##
#################

if(USE_LIBRAW)
    add_definitions(-DUSE_LIBRAW)
    set(LIBS ${LIBS} raw)
endif()

#################
##
##  GDAL
##
#################

if(USE_GDAL)
    add_definitions(-DUSE_GDAL)
    find_package(GDAL REQUIRED)
    include_directories(${GDAL_INCLUDE_DIR})
    set(LIBS ${LIBS} ${GDAL_LIBRARIES})
endif()

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

if (NOT WINDOWS)
	add_definitions(-DLUA_USE_POSIX)
endif()
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
    find_package(SDL2 REQUIRED)

    if (USE_GL3)
        add_definitions(-DGL3)
        set(SOURCES ${SOURCES}
            external/imgui/examples/sdl_opengl3_example/imgui_impl_sdl_gl3.cpp
        )
        include_directories(external/imgui/examples/sdl_opengl3_example)
    else()
        set(SOURCES ${SOURCES}
            external/imgui/examples/sdl_opengl2_example/imgui_impl_sdl_gl2.cpp
        )
        include_directories(external/imgui/examples/sdl_opengl2_example)
    endif()

    set(SOURCES ${SOURCES}
        external/imgui/examples/libs/gl3w/GL/gl3w.c
    )
    include_directories(external/imgui/examples/libs/gl3w)

    if(SDL2_LIBRARIES)
        string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
        set(LIBS ${LIBS} ${SDL2_LIBRARIES})
    else()
        if (WINDOWS)
            set(LIBS ${LIBS} mingw32 SDL2main SDL2)
        else()
            # I don't know why the other solution does not work on manjaro
            set(LIBS ${LIBS} SDL2)
        endif()
    endif()
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
##  globbing on windows
##
#################
if(WINDOWS)
    include_directories(external/glob)
    set(SOURCES ${SOURCES}
        external/glob/glob.c
        external/glob/fnmatch.c
    )
endif()

#################
##
##  VPV
##
#################
set(SOURCES ${SOURCES}
    src/main.cpp
    src/menu.cpp
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
    src/Histogram.cpp
    src/config.cpp
    src/editors.cpp
    src/events.cpp
    src/imgui_custom.cpp
    src/ImageCache.cpp
    src/ImageCollection.cpp
    src/ImageProvider.cpp
    src/LoadingThread.cpp
    src/Terminal.cpp
    src/EditGUI.cpp
    src/icons.cpp
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_demo.cpp
)

if(WINDOWS)
    set(SOURCES ${SOURCES} misc/resources.rc)
endif()

include(GenerateLuaFiles)

if(WINDOWS)
    add_executable(vpv WIN32 ${SOURCES})
else()
    add_executable(vpv ${SOURCES})
endif()

set(LIBS ${LIBS}
    lua
    ${OPENGL_LIBRARIES}
    pthread
    efsw
    z
)
if(NOT WINDOWS)
	set(LIBS ${LIBS} dl)
endif()
target_link_libraries(vpv ${LIBS})

#################
##
##  MISC
##
#################

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vpv DESTINATION bin)


if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   install(FILES misc/vpv.desktop DESTINATION share/applications)
   install(FILES misc/icon.png DESTINATION share/icons/hicolor/48x48/apps RENAME vpv.png)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if(APPLE)
    file(COPY ${CMAKE_SOURCE_DIR}/misc/icon.icns DESTINATION "vpv.app/Contents/Resources/")
    set_target_properties(vpv PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/misc/MacOSXBundleInfo.plist.in)
endif()


if (CMAKE_GENERATOR STREQUAL "Ninja" AND
    ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9) OR
    (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.5)))
    # Force colored warnings in Ninja's output, if the compiler has -fdiagnostics-color support.
    # Rationale in https://github.com/ninja-build/ninja/issues/814
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
endif()

# get a describe of the state of the git
# from http://cmake.3232098.n2.nabble.com/Putting-the-git-commit-hash-in-a-cmake-variable-tp7598423p7598426.html
execute_process(
    COMMAND
    git -C ${CMAKE_SOURCE_DIR} describe --tags --dirty
    RESULT_VARIABLE
    SHORT_HASH_RESULT
    OUTPUT_VARIABLE
    SHORT_HASH)

# If running in script mode (this runs on every build)
if (CMAKE_SCRIPT_MODE_FILE)
    if (EXISTS "${SHORT_HASH_FILE}")
        file(READ ${SHORT_HASH_FILE} READ_IN_SHORT_HASH)
    else()
        set(READ_IN_SHORT_HASH "")
    endif()

    if (NOT ("${READ_IN_SHORT_HASH}" STREQUAL "${SHORT_HASH}"))
        message(STATUS "Short hash is out of date")
        # This will update short_hash.txt, causing cmake to reconfigure
        file(WRITE ${SHORT_HASH_FILE} ${SHORT_HASH})
    endif()

    # Else running as part of cmake configure
else()
    set(SHORT_HASH_FILE ${CMAKE_CURRENT_BINARY_DIR}/short_hash.txt)
    file(WRITE ${SHORT_HASH_FILE} ${SHORT_HASH})

    # The trick here is to make sure short_hash.txt is listed as a byproduct
    add_custom_target(
        git_short_hash
        BYPRODUCTS
        ${SHORT_HASH_FILE}
        COMMAND
        ${CMAKE_COMMAND}
        "-DSHORT_HASH_FILE=${SHORT_HASH_FILE}"
        "-P" "${CMAKE_CURRENT_LIST_FILE}"
        COMMENT
        "Re-checking short hash..."
        VERBATIM
        USES_TERMINAL)

    # This configure_file makes cmake reconfigure dependent on short_hash.txt
    configure_file(${SHORT_HASH_FILE} ${SHORT_HASH_FILE}.junk COPYONLY)

    message(STATUS "Short Hash: ${SHORT_HASH}")
endif()

string(STRIP "${SHORT_HASH}" SHORT_HASH)
set(VPV_VERSION "" CACHE STRING "version string as fallback to the git describe")
if(SHORT_HASH STREQUAL "")
   set(SHORT_HASH ${VPV_VERSION})
endif()
add_definitions(-DVPV_VERSION="${SHORT_HASH}")

