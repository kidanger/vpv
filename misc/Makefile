export CXXFLAGS = -std=c++11 -march=native -O3
export CFLAGS   = -march=native -O3

CPPFLAGS = -Iexternal/imgui          \
           -Iexternal/imgui-sfml     \
           -Iexternal/iio            \
           -Iexternal/nanosvg/src    \
           -Iexternal/lua/src        \
           -Iexternal/imscript       \
           -Iexternal/others         \
           -Iexternal/efsw/include   \

LDLIBS   = -lsfml-system -lsfml-window -lsfml-graphics   \
           -ljpeg -lpng -ltiff -lGL -lpthread -lz

BIN      = vpv
OBJ      = src/main.o                        \
           src/Window.o                      \
           src/Sequence.o                    \
           src/View.o                        \
           src/Player.o                      \
           src/Colormap.o                    \
           src/Image.o                       \
           src/Texture.o                     \
           src/Shader.o                      \
           src/shaders.o                     \
           src/layout.o                      \
           src/watcher.o                     \
           src/wrapplambda.o                 \
           src/SVG.o                         \
           src/config.o                      \
           src/editors.o                     \
           src/luafiles.o                    \
           external/efsw/efsw.a              \
           external/lua/src/liblua.a         \
           external/imgui/imgui.o            \
           external/imgui/imgui_draw.o       \
           external/imgui/imgui_demo.o       \
           external/imgui-sfml/imgui-SFML.o  \
           external/iio/iio.o                \
           #external/gmic/gmic.o

all      : $(BIN)
$(BIN)   : $(OBJ) ; $(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)
clean    :
	$(RM) $(BIN) $(OBJ) luafiles.c
	$(MAKE) -C external/efsw clean
	$(MAKE) -C external/lua/src clean

external/efsw/efsw.a:      ; $(MAKE) -C external/efsw
external/lua/src/liblua.a: ; $(MAKE) -C external/lua/src liblua.a

src/vpvrc.i : vpvrc ; xxd -i $< $@
.deps.mk:
	$(CC) $(CPPFLAGS) -MM src/*.cpp src/*.c | sed "\:^[^ ]:s:^:src/:g" > $@
-include .deps.mk
