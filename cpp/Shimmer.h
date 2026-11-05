#pragma once

#include "Types.h"
#include <string>
#include <memory>

class Game;

// Golden cookie / reindeer / wrath cookie shimmer
class Shimmer {
public:
    std::string type;      // "golden", "reindeer", "wrath", ...
    f64 x = 0.0, y = 0.0;
    f64 dur = 0.0;         // lifetime
    f64 age = 0.0;
    bool dead = false;

    Shimmer(std::string type, f64 x, f64 y, f64 dur);
    void tick(f64 dt);
};
