export CFLAGS   = -march=native -O3
export CXXFLAGS = -std=c++11 $(CFLAGS)

CPPFLAGS = -Iexternal/imgui -Iexternal/imgui-sfml -Iexternal/iio \
           -Iexternal/efsw/include -Iexternal/nanosvg/src -Iexternal/lua/src \
           -Iexternal/imscript -Iexternal/others -Iexternal/kaguya/include \
           -Iexternal/imgui/misc/fonts -Iexternal/imgui/examples/libs/gl3w \
           -Iexternal/imgui/examples/sdl_opengl3_example \
           $(shell pkg-config --cflags gdal)
CPPFLAGS += -DSDL -DUSE_GDAL -DGL3
LDLIBS   = -lSDL2 -ljpeg -lpng -ltiff -lGL -lpthread -lz -ldl -lgdal

SRC      = $(shell ls src/*.cpp)

BIN      = vpv
OBJ      = $(SRC:.cpp=.o) \
           src/wrapplambda.o src/luafiles.o external/imgui/imgui.o \
           external/imgui/imgui_draw.o external/imgui/imgui_demo.o \
           external/iio/iio.o external/efsw/efsw.a external/lua/src/liblua.a \
           external/imgui/examples/sdl_opengl3_example/imgui_impl_sdl_gl3.o \
           external/imgui/examples/libs/gl3w/GL/gl3w.o

$(BIN)   : $(OBJ) ; $(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

clean    :
	$(RM) $(BIN) $(OBJ) src/luafiles.c
	$(MAKE) -C external/efsw clean
	$(MAKE) -C external/lua/src clean

external/efsw/efsw.a:      ; $(MAKE) -C external/efsw
external/lua/src/liblua.a: ; $(MAKE) -C external/lua/src posix

src/luafiles.c: vpvrc ; bash misc/genluafiles.sh $^ >$@

.deps.mk:         ; $(CXX) $(CPPFLAGS) -MM src/*.cpp |sed '/^[^ ]/s/^/src\//'>$@
-include .deps.mk
