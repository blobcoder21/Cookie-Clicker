#pragma once

#include "Types.h"
#include <string>
#include <memory>

struct BuffType; // forward (registry of buff kinds)

// Mirrors a running buff instance (e.g. Click frenzy, Elder frenzy)
class Buff {
public:
    std::shared_ptr<BuffType> type;
    f64 maxTime = 0.0;
    f64 time = 0.0;
    f64 arg1 = 0.0, arg2 = 0.0, arg3 = 0.0;

    bool active() const { return time > 0.0; }
    void tick(f64 dt) { if (time > 0.0) time -= dt; }
};

struct BuffType {
    i32 id = 0;
    std::string name;
    bool vanilla = true;
    // multClick, multCps, etc. would live here
};
