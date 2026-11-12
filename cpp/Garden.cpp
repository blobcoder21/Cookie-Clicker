#include "Garden.h"
#include "Game.h"
#include <algorithm>
#include <cmath>
#include <sstream>

Garden::Garden(Building* parent) { this->parent = parent; name = "Garden"; }

void Garden::registerPlants() {
    // Starter plant set (mirrors JS Garden median/basic plants). id 0 = empty.
    plantDefs[1] = {1, "Medicinal", 0.05, 60.0, 0.0001};
    plantDefs[2] = {2, "Glove", 0.08, 90.0, 0.0001};
    plantDefs[3] = {3, "Crumb", 0.12, 120.0, 0.0001};
    plantDefs[4] = {4, "Bakeberry", 0.20, 180.0, 0.0002};
    plantDefs[5] = {5, "Chocoroot", 0.15, 150.0, 0.0002};
}

void Garden::init() {
    registerPlants();
    plots.fill(Plot{});
    loaded = true;
    unlocked = true;
    level = 1;
    // Seed from the game seed so mutations are deterministic across runs.
    u32 seed = 0;
    if (g_game) {
        for (char c : g_game->seed) seed = seed * 31u + static_cast<u8>(c);
    }
    rng_.seed(seed);
}

i32 Garden::plantAt(i32 x, i32 y) const {
    if (!valid(x, y)) return -1;
    return plots[indexOf(x, y)].plantId;
}

i32 Garden::growAt(i32 x, i32 y) const {
    if (!valid(x, y)) return 0;
    return static_cast<i32>(plots[indexOf(x, y)].growth * 100.0);
}

f64 Garden::plantGrowRate(i32 plantId) const {
    auto it = plantDefs.find(plantId);
    if (it == plantDefs.end()) return 0.0;
    // 1.0 / growTime per second -> per-frame at GameConst::FPS
    return 1.0 / (it->second.growTime * static_cast<f64>(GameConst::FPS));
}

bool Garden::plant(i32 x, i32 y, i32 plantId) {
    if (!valid(x, y)) return false;
    if (plantDefs.find(plantId) == plantDefs.end()) return false;
    auto& p = plots[indexOf(x, y)];
    if (p.plantId != -1) return false;
    p.plantId = plantId;
    p.growth = 0.0;
    p.age = 0;
    return true;
}

bool Garden::harvest(i32 x, i32 y) {
    if (!valid(x, y)) return false;
    auto& p = plots[indexOf(x, y)];
    if (p.plantId == -1) return false;
    auto it = plantDefs.find(p.plantId);
    if (it == plantDefs.end()) return false;
    if (p.growth < 1.0) return false; // not mature

    // Mature harvest yields cookies proportional to plant bonus.
    f64 reward = it->second.baseCookieBonus * 50.0 * static_cast<f64>(GameConst::FPS);
    if (g_game) g_game->earn(reward);

    p = Plot{}; // empty the plot
    return true;
}

bool Garden::mutate(i32 x, i32 y) {
    if (!valid(x, y)) return false;
    auto& p = plots[indexOf(x, y)];
    if (p.plantId == -1) return false;
    if (p.growth < 1.0) return false; // only mature plants mutate
    return tryMutate(x, y);
}

bool Garden::tryMutate(i32 x, i32 y) {
    auto& p = plots[indexOf(x, y)];
    auto it = plantDefs.find(p.plantId);
    if (it == plantDefs.end()) return false;

    // Collect neighboring plant ids
    i32 neighbors[4] = {-1, -1, -1, -1};
    i32 nCount = 0;
    if (valid(x - 1, y)) neighbors[nCount++] = plots[indexOf(x - 1, y)].plantId;
    if (valid(x + 1, y)) neighbors[nCount++] = plots[indexOf(x + 1, y)].plantId;
    if (valid(x, y - 1)) neighbors[nCount++] = plots[indexOf(x, y - 1)].plantId;
    if (valid(x, y + 1)) neighbors[nCount++] = plots[indexOf(x, y + 1)].plantId;

    // Mutation chance scales with number of different neighbors
    i32 diffNeighbors = 0;
    for (i32 i = 0; i < nCount; ++i) {
        if (neighbors[i] != -1 && neighbors[i] != p.plantId) ++diffNeighbors;
    }
    if (diffNeighbors == 0) return false;

    f64 chance = it->second.mutationChance * static_cast<f64>(diffNeighbors);
    if (rng_.random() < chance) {
        // Advance plant id to next tier mutation (simple deterministic progression)
        i32 newId = p.plantId + 1;
        if (plantDefs.find(newId) != plantDefs.end()) {
            p.plantId = newId;
            p.growth = 0.0; // reset growth for the new plant
            return true;
        }
    }
    return false;
}

void Garden::eachFrame() {
    if (!loaded || !unlocked) return;

    for (i32 i = 0; i < totalPlots; ++i) {
        auto& p = plots[i];
        if (p.plantId == -1) continue;

        f64 rate = plantGrowRate(p.plantId);
        p.growth += rate;
        if (p.growth > 1.0) p.growth = 1.0;
        ++p.age;

        // Attempt mutation on mature plants
        if (p.growth >= 1.0) {
            i32 x = i % gridW;
            i32 y = i / gridW;
            tryMutate(x, y);
        }
    }
}

f64 Garden::cookieBonus() const {
    if (!loaded || !unlocked) return 1.0;
    f64 bonus = 0.0;
    for (const auto& p : plots) {
        if (p.plantId == -1 || p.growth < 1.0) continue;
        auto it = plantDefs.find(p.plantId);
        if (it != plantDefs.end()) bonus += it->second.baseCookieBonus;
    }
    return 1.0 + bonus;
}

f64 Garden::clickBonus() const {
    return 0.0; // garden doesn't affect clicking directly
}

i32 Garden::maxSlots() const { return totalPlots; }
i32 Garden::slotCount() const {
    i32 n = 0;
    for (const auto& p : plots) if (p.plantId != -1) ++n;
    return n;
}

std::string Garden::save() {
    std::ostringstream s;
    s << level << ";";
    for (i32 i = 0; i < totalPlots; ++i) {
        const auto& p = plots[i];
        s << p.plantId << "," << static_cast<i32>(p.growth * 100.0) << ";";
    }
    return s.str();
}

void Garden::load(const std::string& data) {
    // Format: level;plantId,growthPct;plantId,growthPct;...
    std::istringstream s(data);
    std::string token;
    if (std::getline(s, token, ';')) level = std::stoi(token);
    for (i32 i = 0; i < totalPlots; ++i) {
        auto& p = plots[i];
        p = Plot{};
        if (!std::getline(s, token, ';')) break;
        size_t comma = token.find(',');
        if (comma == std::string::npos) continue;
        p.plantId = std::stoi(token.substr(0, comma));
        int growthPct = std::stoi(token.substr(comma + 1));
        p.growth = static_cast<f64>(growthPct) / 100.0;
        if (p.growth > 1.0) p.growth = 1.0;
    }
    loaded = true;
    unlocked = true;
}
