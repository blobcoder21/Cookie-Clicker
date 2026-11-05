#pragma once

#include "Types.h"
#include <string>

class Game;

// Mirrors a Cookie Clicker achievement
class Achievement {
public:
    i32 id = 0;
    std::string name;
    std::string desc;
    std::string icon;
    bool won = false;
    bool vanilla = true;

    Achievement(std::string name, std::string desc, std::string icon = "");
};
