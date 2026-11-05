#pragma once

#include "Types.h"

struct Particle {
    f64 x = 0.0, y = 0.0;
    f64 vx = 0.0, vy = 0.0;
    f64 size = 0.0;
    f64 age = 0.0;
    f64 duration = 0.0;
    bool dead = false;

    void tick(f64 dt);
};
