export CXXFLAGS = -std=c++11 -march=native -O3
export CFLAGS   = -march=native -O3
CPPFLAGS = -Iexternal/imgui \
           -Iexternal/imgui-sfml \
           -Iexternal/iio \
           -Iexternal/efsw/include \
           -Iexternal/nanosvg/src \
           -Iexternal/lua/src \
           -Iexternal/imscript \
           -Iexternal/others \
           -Iexternal/kaguya/include \
           -Iexternal/imgui/misc/fonts \
           -Iexternal/imgui/examples/libs/gl3w \
           -Iexternal/imgui/examples/sdl_opengl2_example
CPPFLAGS += -DSDL
LDLIBS   = -lSDL2 -ljpeg -lpng -ltiff -lGL -lpthread -lz -ldl

BIN      = vpv
OBJ      = external/imgui/imgui.o \
           external/imgui/imgui_draw.o \
           external/imgui/imgui_demo.o \
           external/iio/iio.o \
           src/main.o \
           src/Window.o \
           src/Sequence.o    \
           src/View.o \
           src/Player.o \
           src/Colormap.o \
           src/Image.o \
           src/Texture.o \
           src/Shader.o \
           src/shaders.o \
           src/layout.o \
           src/watcher.o \
           src/wrapplambda.o \
           src/SVG.o \
           src/config.o \
           src/editors.o \
           src/events.o \
           src/imgui_custom.o \
           luafiles.o \
           external/efsw/efsw.a \
           external/lua/src/liblua.a \
           external/imgui/examples/sdl_opengl2_example/imgui_impl_sdl_gl2.o \
           external/imgui/examples/libs/gl3w/GL/gl3w.o \
           #external/gmic/gmic.o

all      : $(BIN)
$(BIN)   : $(OBJ) ; $(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)
clean    :
	$(RM) $(BIN) $(OBJ) luafiles.c
	$(MAKE) -C external/efsw clean
	$(MAKE) -C external/lua/src clean

external/efsw/efsw.a:      ; $(MAKE) -C external/efsw
external/lua/src/liblua.a: ; $(MAKE) -C external/lua/src liblua.a

luafiles.c: vpvrc
	bash misc/genluafiles.sh $^ >$@


.deps.mk:         ; $(CC) $(CPPFLAGS) -MM *.cpp > $@
-include .deps.mk
