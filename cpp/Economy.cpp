#include "Economy.h"
#include "Game.h"

namespace Economy {
    void calculateGains(Game& game) { game.calculateGains(); }
    f64 mouseCps(Game& game) { return game.mouseCps(); }
}
