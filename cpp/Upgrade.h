#pragma once

#include "Types.h"
#include <string>
#include <functional>

class Building;
class Game;

// Mirrors JS Game.Upgrade
class Upgrade {
public:
    i32 id = 0;
    std::string name;
    std::string desc;
    f64 price = 0.0;
    std::string icon; // "row,col" sprite coordinate
    std::function<bool()> buyFunction;

    bool unlocked = false;
    bool bought = false;
    bool vanilla = true;

    // tiered upgrade linkage
    Building* tieredBuilding = nullptr;
    i32 tier = 0;

    Upgrade(std::string name, std::string desc, f64 price, std::string icon,
            std::function<bool()> buyFunc = nullptr);
    bool canBuy() const;
    f64 getPrice() const { return price; }
};
