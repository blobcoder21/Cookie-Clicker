#pragma once

#include "Types.h"
#include <string>
#include <functional>

class Building;
class Game;

// Base class for building minigames (Garden, Grimoire, Pantheon, Dungeons).
// Pure logic: no Render/UI dependencies. draw() is a stub for the deferred UI.
class Minigame {
public:
    Building* parent = nullptr;
    std::string name;
    bool loaded = false;
    bool unlocked = false;     // minigame unlocked for the parent building
    i32 level = 0;             // minigame level (unlocks more slots/content)

    virtual ~Minigame() = default;
    virtual void init() = 0;
    virtual void draw() {}                 // stubbed — UI deferred
    virtual void eachFrame() {}            // per-frame simulation
    virtual std::string save() { return ""; }   // minigame save string
    virtual void load(const std::string& /*data*/) {}  // restore from save
    virtual void onResize() {}

    // Cookie production bonus applied by this minigame (added to global CPS mult).
    virtual f64 cookieBonus() const { return 0.0; }

    // Click power bonus applied by this minigame (added to mouse CPS mult).
    virtual f64 clickBonus() const { return 0.0; }

    // Maximum number of active slots/plots/spells this minigame supports.
    virtual i32 maxSlots() const { return 0; }
    virtual i32 slotCount() const { return 0; }
};
