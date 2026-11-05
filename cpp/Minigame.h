#pragma once

#include "Types.h"
#include <string>
#include <functional>

class Building;
class Game;

// Base class for building minigames (Garden, Grimoire, Pantheon, Dungeons)
class Minigame {
public:
    Building* parent = nullptr;
    std::string name;
    bool loaded = false;

    virtual ~Minigame() = default;
    virtual void init() = 0;
    virtual void draw() {}
    virtual void eachFrame() {}
    virtual std::string save() { return ""; }
    virtual void onResize() {}
};
