export CFLAGS   = -march=native -O3
export CXXFLAGS = $(CFLAGS)

CPPFLAGS = -Iexternal/imgui -Iexternal/imgui-sfml -Iexternal/iio \
           -Iexternal/efsw/include -Iexternal/nanosvg/src -Iexternal/lua/src \
           -Iexternal/imscript -Iexternal/others -Iexternal/kaguya/include \
           -Iexternal/imgui/misc/fonts -Iexternal/imgui/examples/libs/gl3w \
           -Iexternal/imgui/examples/sdl_opengl3_example \
           -Iexternal/doctest -Iexternal/ghc \
           -Iexternal/reproc/reproc++/include
CPPFLAGS += $(shell pkg-config sdl2 --cflags)
#CPPFLAGS += -DDOCTEST_CONFIG_DISABLE
CPPFLAGS += -DUSE_IIO
CPPFLAGS += -DUSE_IIO_NPY
CPPFLAGS += -DUSE_PLAMBDA
CPPFLAGS += -DSDL -DGL3 -DVPV_VERSION=\"$(shell git describe --tags)\"
LDLIBS   = -lSDL2 -ljpeg -lpng -ltiff -lpthread -lz -ldl


SRC      = $(shell ls src/*.cpp | grep -v tests | grep -v uzzy)

BIN      = vpv
OBJ      = $(SRC:.cpp=.o) luafiles.o \
           src/wrapplambda.o external/imgui/imgui.o \
           external/imgui/imgui_draw.o external/imgui/imgui_demo.o \
           external/iio/iio.o external/efsw/efsw.a external/lua/src/liblua.a \
           external/imgui/examples/sdl_opengl3_example/imgui_impl_sdl_gl3.o \
           external/imgui/examples/libs/gl3w/GL/gl3w.o external/iio/npy.o \
           external/reproc/reproc.a

$(BIN)   : $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

clean    :
	$(RM) $(BIN) $(OBJ) luafiles.cpp
	$(MAKE) -C external/efsw clean
	$(MAKE) -C external/reproc clean
	$(MAKE) -C external/lua/src clean

external/efsw/efsw.a:
	$(MAKE) -C external/efsw

external/reproc/reproc.a:
	$(MAKE) -C external/reproc

external/lua/src/liblua.a:
	$(MAKE) -C external/lua/src posix CC="$(CC)" MYCFLAGS="$(CFLAGS)"

luafiles.cpp: vpvrc
	bash misc/genluafiles.sh $^ >$@

.deps.mk:
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MM src/*.cpp |sed '/^[^ ]/s/^/src\//'>$@
-include .deps.mk


# hack around macOS shenanigans
ifeq ($(shell uname),Darwin)
CXXFLAGS += -std=c++17
LDLIBS += -framework OpenGL -framework CoreFoundation -framework CoreServices
else
LDLIBS += -lGL
endif
