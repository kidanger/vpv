cmake_minimum_required(VERSION 3.7...3.20)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(vpv
    LANGUAGES CXX C
    DESCRIPTION "Image viewer designed for image processing experts."
)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/modules/")

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
        STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
   include(CTest)
endif()

#################
##
##  OPTIONS
##
#################

option(USE_EXR "compile with OpenEXR support (for iio)" OFF)
option(USE_OCTAVE "compile with octave support" OFF)
option(USE_LIBRAW "compile with LibRAW support" OFF)
option(USE_GDAL "compile with GDAL support" OFF)
option(USE_IIO "compile with IIO support" ON)
option(USE_IIO_NPY "compile with IIO numpy support" ON)
option(USE_PLAMBDA "compile with plambda support" ON)

if(MSYS OR MSVC)
	set(WINDOWS 1)
	add_definitions(-DWINDOWS)
endif()

#################
##
##  FLAGS
##
#################

include(TargetEnableIPOIfSupported)

set(CMAKE_CXX_STANDARD 17)
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

include_directories(SYSTEM external/imscript)
include_directories(SYSTEM external/others)
include_directories(SYSTEM external/nanosvg/src)
include_directories(SYSTEM external/lua/src)
include_directories(SYSTEM external/kaguya/include)

if(NOT WINDOWS)
   if(POLICY CMP0072)
      cmake_policy(SET CMP0072 NEW)
   endif()
   find_package(OpenGL REQUIRED)
   include_directories(SYSTEM ${OPENGL_INCLUDE_DIR})
else()
   set(OPENGL_LIBRARIES opengl32)
endif()

set(LIBS ${LIBS} ${OPENGL_LIBRARIES})

if((CMAKE_CXX_COMPILER_ID MATCHES "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9.1.0"))
    set(LIBS ${LIBS} stdc++fs)
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

    include_directories(SYSTEM ${OCTAVE_INCLUDE_DIRS}/..)
    if(OCTAVE_VERSION_STRING STREQUAL "4.2.2")
        include_directories(SYSTEM ${OCTAVE_INCLUDE_DIRS})
    endif()

    set(LIBS ${LIBS} ${OCTAVE_OCTINTERP_LIBRARY} ${OCTAVE_OCTAVE_LIBRARY})
endif()


#################
##
##  IIO
##
#################

if(USE_IIO)
    add_definitions(-DUSE_IIO)
    if(USE_IIO_NPY)
        add_definitions(-DUSE_IIO_NPY)
        add_library(iio STATIC external/iio/iio.c external/iio/npy.c)
    else()
        add_library(iio STATIC external/iio/iio.c)
    endif()
    include_directories(SYSTEM external/iio)

    if(USE_EXR)
	    find_package(OpenEXR REQUIRED)
	    add_definitions(-DI_CAN_HAS_LIBEXR) # for iio
            include_directories(SYSTEM ${OPENEXR_INCLUDE_PATHS})
	    target_link_libraries(iio ${OPENEXR_LIBRARIES})
    endif()

    target_enable_ipo_if_supported(iio)

    set(LIBS ${LIBS} iio)
endif()

find_package(PNG REQUIRED)
include_directories(SYSTEM ${PNG_INCLUDE_DIRS})
set(LIBS ${LIBS} ${PNG_LIBRARIES})

find_package(TIFF REQUIRED)
include_directories(SYSTEM ${TIFF_INCLUDE_DIRS})
set(LIBS ${LIBS} ${TIFF_LIBRARIES})

find_package(JPEG REQUIRED)
include_directories(SYSTEM ${JPEG_INCLUDE_DIRS})
set(LIBS ${LIBS} ${JPEG_LIBRARIES})


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
    include_directories(SYSTEM ${GDAL_INCLUDE_DIR})
    set(LIBS ${LIBS} ${GDAL_LIBRARIES})
endif()

#################
##
##  EFSW
##
#################

find_package(EFSW QUIET)
if(EFSW_FOUND)
    include_directories(SYSTEM ${EFSW_INCLUDE_DIRS})
    set(EFSW_LIBRARIES efsw::efsw)
else()
    set(STATIC_LIB ON CACHE BOOL "Build efsw as a static library" FORCE)
    add_subdirectory(external/efsw)
    include_directories(SYSTEM external/efsw/include)
    set(EFSW_LIBRARIES efsw)
endif()
set(LIBS ${LIBS} ${EFSW_LIBRARIES})

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
target_enable_ipo_if_supported(lua)
set(LIBS ${LIBS} lua)

#################
##
##  SDL
##
#################

find_package(SDL2 REQUIRED)
if(MSVC)
   set(SDL_LIBS SDL2::SDL2 SDL2::SDL2main)
elseif(MSYS)
   set(SDL_LIBS mingw32 SDL2main SDL2)
elseif(APPLE)
   set(SDL_LIBS SDL2::SDL2 SDL2::SDL2main)
else()
   # maybe one of these will work?
   if(TARGET SDL2::SDL2)
      set(SDL_LIBS SDL2::SDL2)
      # SDL2main is not needed on linux (and broken on fedora)
   else()
      if(SDL2_LIBRARIES)
         string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)
      endif()
      set(SDL_LIBS SDL2 ${SDL2_LIBRARY} ${SDL2_LIBRARIES})
      include_directories(SYSTEM ${SDL2_INCLUDE_DIR})
      include_directories(SYSTEM ${SDL2_INCLUDE_DIRS})
      include_directories(SYSTEM /usr/include/SDL2)
   endif()
endif()
set(LIBS ${LIBS} ${SDL_LIBS})


#################
##
##  globbing on windows
##
#################
if(WINDOWS)
    add_library(glob STATIC
        external/glob/glob.c
        external/glob/fnmatch.c
    )
    if(MSVC)
        target_include_directories(glob SYSTEM PRIVATE external/dirent)
    endif()
    target_include_directories(glob SYSTEM PUBLIC external/glob)
    target_compile_definitions(glob PRIVATE HAVE_STRING_H HAVE_STRDUP)
    target_enable_ipo_if_supported(glob)

    set(LIBS ${LIBS} glob)
endif()

#################
##
##  reproc
##
#################
set(REPROC++ ON)
set(REPROC_INSTALL OFF)
add_subdirectory(external/reproc)
set(LIBS ${LIBS} reproc++)

#################
##
##  ghc
##
#################
include_directories(SYSTEM external/ghc)

#################
##
##  imgui
##
#################
add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/examples/sdl_opengl3_example/imgui_impl_sdl_gl3.cpp
)
target_include_directories(imgui SYSTEM PUBLIC external/imgui/examples/sdl_opengl3_example)
target_include_directories(imgui SYSTEM PUBLIC external/imgui)
target_include_directories(imgui SYSTEM PUBLIC external/imgui/misc/fonts)
target_include_directories(imgui SYSTEM PUBLIC external/imgui/examples/libs/gl3w)
target_link_libraries(imgui ${SDL_LIBS})
target_enable_ipo_if_supported(imgui)

set(LIBS ${LIBS} imgui)

#################
##
##  DOCTEST
##
#################

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE external/doctest)

set(LIBS ${LIBS} doctest)

#################
##
##  Rust :o
##
#################

add_subdirectory(fuzzy-finder)
set(LIBS ${LIBS} murkycxx)

#################
##
##  LIBVPV
##
#################
set(SOURCES ${SOURCES}
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
    src/OpenGLDebug.cpp
    src/collection_expression.cpp
    src/strutils.cpp
    src/globals.cpp
    src/dragndrop.cpp
    external/imgui/examples/libs/gl3w/GL/gl3w.c
)
include(GenerateLuaFiles)

if(USE_PLAMBDA)
    add_definitions(-DUSE_PLAMBDA)
    set(SOURCES ${SOURCES}
        src/wrapplambda.c
    )
endif()

add_library(libvpv OBJECT ${SOURCES})

if(NOT MSVC)
	set(LIBS ${LIBS} pthread)
endif()
if(NOT WINDOWS)
	set(LIBS ${LIBS} ${CMAKE_DL_LIBS})
endif()
target_link_libraries(libvpv PUBLIC ${LIBS})

include(VPVFlags)
target_compile_options(libvpv PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:$<$<CONFIG:DEBUG>:${VPV_CXX_FLAGS}>>")
# Needed on Windows so that windows.h does not redefine min/max which conflicts with std::max/std::min
target_compile_definitions(libvpv PRIVATE "$<$<BOOL:${WINDOWS}>:NOMINMAX>")
target_compile_definitions(libvpv PRIVATE "$<$<BOOL:${WINDOWS}>:WIN32_LEAN_AND_MEAN>")
target_enable_ipo_if_supported(libvpv)


#################
##
##  VPV
##
#################

if(WINDOWS)
    add_executable(vpv WIN32 src/main.cpp misc/resources.rc $<TARGET_OBJECTS:libvpv>)
else()
    add_executable(vpv src/main.cpp $<TARGET_OBJECTS:libvpv>)
endif()

target_link_libraries(vpv PRIVATE ${LIBS})

target_compile_options(vpv PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:$<$<CONFIG:DEBUG>:${VPV_CXX_FLAGS}>>")
# Needed on Windows so that windows.h does not redefine min/max which conflicts with std::max/std::min
target_compile_definitions(vpv PRIVATE "$<$<BOOL:${WINDOWS}>:NOMINMAX>")
target_compile_definitions(vpv PRIVATE "$<$<BOOL:${WINDOWS}>:WIN32_LEAN_AND_MEAN>")
target_enable_ipo_if_supported(vpv)


#################
##
##  TESTS
##
#################

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME AND BUILD_TESTING AND CMAKE_BUILD_TYPE STREQUAL "Debug")
   if(WINDOWS)
      add_executable(tests WIN32 src/tests_runner.cpp $<TARGET_OBJECTS:libvpv>)
   else()
      add_executable(tests src/tests_runner.cpp $<TARGET_OBJECTS:libvpv>)
   endif()
   target_link_libraries(tests PRIVATE ${LIBS})
   add_test(NAME tests COMMAND tests)
else()
   target_compile_definitions(libvpv PRIVATE DOCTEST_CONFIG_DISABLE)
   target_compile_definitions(vpv PRIVATE DOCTEST_CONFIG_DISABLE)
endif()


#################
##
##  MISC
##
#################


if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   install(FILES misc/vpv.desktop DESTINATION share/applications)
   install(FILES misc/icon.png DESTINATION share/icons/hicolor/48x48/apps RENAME vpv.png)
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if(APPLE)
    file(COPY ${CMAKE_SOURCE_DIR}/misc/icon.icns DESTINATION "vpv.app/Contents/Resources/")
    set_target_properties(vpv PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/misc/MacOSXBundleInfo.plist.in)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vpv.app/Contents/MacOS/vpv DESTINATION bin)
else()
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/vpv DESTINATION bin)
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


# FIXME: Until the code below is fixed, it needs to be disabled for Ninja (it keeps rebuilding the project)
#        and for Visual Studio (cyclic dependencies issue).
if (NOT CMAKE_GENERATOR MATCHES "Visual Studio" AND NOT CMAKE_GENERATOR MATCHES "Ninja")
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
endif()

string(STRIP "${SHORT_HASH}" SHORT_HASH)
set(VPV_VERSION "" CACHE STRING "version string as fallback to the git describe")
if(SHORT_HASH STREQUAL "")
   set(SHORT_HASH ${VPV_VERSION})
endif()
target_compile_definitions(vpv PRIVATE VPV_VERSION="${SHORT_HASH}")

