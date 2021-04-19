#pragma once

#include <kaguya/kaguya.hpp>
#include <string>

namespace config {

void load();

float get_float(const std::string& name);
bool get_bool(const std::string& name);
int get_int(const std::string& name);
std::string get_string(const std::string& name);
void load_shaders();

kaguya::State& get_lua();

}

struct Sequence;
struct Colormap;
struct Player;
struct View;
struct Window;
std::shared_ptr<Sequence> newSequence(std::shared_ptr<Colormap> c, std::shared_ptr<Player> p, std::shared_ptr<View> v);
std::shared_ptr<View> newView();
std::shared_ptr<Player> newPlayer();
std::shared_ptr<Colormap> newColormap();
std::shared_ptr<Window> newWindow();
