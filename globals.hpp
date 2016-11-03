#pragma once

#include <vector>

namespace sf {
    class RenderWindow;
}

class Sequence;
class View;
class Player;
class Window;

extern sf::RenderWindow* SFMLWindow;
extern std::vector<Sequence*> gSequences;
extern std::vector<View*> gViews;
extern std::vector<Player*> gPlayers;
extern std::vector<Window*> gWindows;

