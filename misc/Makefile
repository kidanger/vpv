CXXFLAGS = -std=c++11 -O2
CPPFLAGS = -Iimgui -Iimgui-sfml -Iiio -Iefsw/include -Dgmic_build
LDLIBS   = -lsfml-system -lsfml-window -lsfml-graphics   \
           -ljpeg -lpng -ltiff -lGL -lpthread -lz

BIN      = vpv
OBJ      = imgui/imgui.o imgui/imgui_draw.o imgui/imgui_demo.o             \
           imgui-sfml/imgui-SFML.o iio/iio.o main.o Window.o Sequence.o    \
           View.o Player.o Colormap.o Image.o Texture.o Shader.o shaders.o \
           layout.o watcher.o wrapplambda.o gmic/gmic.o efsw/efsw.a

all      : $(BIN)
$(BIN)   : $(OBJ) ; $(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)
clean    :        ; $(RM) $(BIN) $(OBJ)

efsw/efsw.a: ; $(MAKE) -C efsw
