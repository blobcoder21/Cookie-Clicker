#pragma once

#include "Types.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

class Game;

// ---------------------------------------------------------------------------
// Mirrors JS Game.Object (a building type). One instance per building kind.
// ---------------------------------------------------------------------------
class Building {
public:
    i32 id = 0;
    std::string name;
    std::string displayName;
    std::string single;   // singular common name
    std::string plural;
    std::string actionName;
    std::string extraName;
    std::string extraPlural;
    std::string desc;

    f64 basePrice = 0.0;
    f64 price = 0.0;
    f64 bulkPrice = 0.0;
    f64 cps = 0.0;
    f64 baseCps = 0.0;

    // icon / art (port of {base, pic, bg, xV, yV, w, rows, x, y, frames})
    std::string icon;
    i32 iconColumn = 0;
    struct Art {
        std::string base;      // e.g. "farm" -> farm.png / farmBackground.png
        std::string pic;
        std::string bg;
        f64 xV = 0, yV = 0, w = 0;
        i32 rows = 0, x = 0, y = 0;
        i32 frames = 1;
    } art;

    std::function<void(Building*)> buyFunction;

    bool locked = true;
    i32 level = 0;
    bool vanilla = true;

    // tiered upgrade / achievement links
    std::vector<class Upgrade*> tieredUpgrades;
    std::vector<class Achievement*> tieredAchievs;
    // synergy with other buildings (building pointer -> multiplier cb)
    using Synergy = std::pair<Building*, std::function<f64()>>;
    std::vector<Synergy> synergies;

    // runtime state
    i32 amount = 0;
    i32 bought = 0;
    i32 free = 0;
    f64 totalCookies = 0.0;
    f64 storedCps = 0.0;
    f64 storedTotalCps = 0.0;
    i32 highest = 0;              // JS building entry field 6

    bool mouseOn = false;
    f64 mousePos[2] = {-100.0, -100.0};

    i32 fortune = 0;

    // minigame
    std::string minigameUrl;
    std::string minigameName;
    std::string minigameSave;    // raw minigame save string (re-emitted on write)
    bool onMinigame = false;
    bool minigameLoaded = false;
    class Minigame* minigame = nullptr;

    bool muted = false;

    // DOM handle analogue (UI row id)
    std::string uiRowId;

    std::function<void()> eachFrame;

    // CPS function override (mirrors JS per-building cps function).
    // When set, storedCps = cpsFunc(this) instead of baseCps.
    std::function<f64(Building*)> cpsFunc;

    Building(std::string name, std::string commonName, std::string desc,
             std::string icon, i32 iconColumn, Art art,
             f64 price, f64 cps, std::function<void(Building*)> buyFunc);

    // economy
    f64 getPrice(i32 n = -1) const;
    f64 getSumPrice(i32 amount) const;
    f64 getReverseSumPrice(i32 amount) const;
    f64 getSellMultiplier() const;
    void buy(i32 amount = 1);
    void sell(i32 amount = 1, bool bypass = false);
    void refresh();
    void switchMinigame(bool on);          // on == -1 toggles
};
