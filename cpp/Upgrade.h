#pragma once

#include "Types.h"
#include <string>
#include <vector>
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

    // Upgrade pool: "", "cookie", "toggle", "debug", "prestige", "tech", etc.
    // Prestige-pool upgrades are bought with heavenly chips and are exempt from
    // ordinary cookie purchase discounts.
    std::string pool = "";

    // Prerequisite upgrades (by name) that must be bought before this one can
    // be purchased. Supports multiple parents and dependency graphs.
    std::vector<std::string> parents;

    // Visibility condition (JS `showIf`). When set, the upgrade only appears in
    // the ascend tree if this returns true.
    std::function<bool()> showIf;

    // When true, this upgrade cannot be placed in a permanent upgrade slot.
    bool noPerm = false;

    // tiered upgrade linkage
    Building* tieredBuilding = nullptr;
    i32 tier = 0;

    Upgrade(std::string name, std::string desc, f64 price, std::string icon,
            std::function<bool()> buyFunc = nullptr);
    bool canBuy() const;
    f64 getPrice() const { return price; }
};
