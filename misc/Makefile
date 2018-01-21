export CXXFLAGS = -std=c++11 -march=native -O3
export CFLAGS   = -march=native -O3
CPPFLAGS = -Iimgui -Iimgui-sfml -Iiio -Iefsw/include -Inanosvg/src -Ilua/src
LDLIBS   = -lsfml-system -lsfml-window -lsfml-graphics   \
           -ljpeg -lpng -ltiff -lGL -lpthread -lz

BIN      = vpv
OBJ      = imgui/imgui.o imgui/imgui_draw.o imgui/imgui_demo.o             \
           imgui-sfml/imgui-SFML.o iio/iio.o main.o Window.o Sequence.o    \
           View.o Player.o Colormap.o Image.o Texture.o Shader.o shaders.o \
           layout.o watcher.o wrapplambda.o SVG.o config.o editors.o \
           luafiles.o efsw/efsw.a lua/src/liblua.a #gmic/gmic.o

all      : $(BIN)
$(BIN)   : $(OBJ) ; $(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)
clean    :
	$(RM) $(BIN) $(OBJ) luafiles.c
	$(MAKE) -C efsw clean
	$(MAKE) -C lua/src clean

efsw/efsw.a:      ; $(MAKE) -C efsw
lua/src/liblua.a: ; $(MAKE) -C lua/src liblua.a

luafiles.c: vpvrc
	bash misc/genluafiles.sh $^ >$@


.deps.mk:         ; $(CC) $(CPPFLAGS) -MM *.cpp > $@
-include .deps.mk
