#pragma once

#include "Types.h"

class Game;

// Placeholder for the full port of Game.CalculateGains / mouseCps / ClickCookie.
// See main.js sections "COOKIE ECONOMICS" and "CPS RECALCULATOR".
namespace Economy {
    void calculateGains(Game& game);
    f64 mouseCps(Game& game);
}
