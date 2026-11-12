#include "Dungeons.h"
#include "Game.h"
#include <algorithm>
#include <sstream>

Dungeons::Dungeons(Building* parent) { this->parent = parent; name = "Dungeons"; }

void Dungeons::registerDungeons() {
    // Starter dungeon set (names come from DungeonGen where available).
    const struct {
        i32 id;
        const char* name;
        i32 maxFloors;
        f64 rewardPerFloor;
    } defs[] = {
        {1, "Mysterious dungeon", 5, 1000.0},
        {2, "Cookie catacombs", 10, 5000.0},
        {3, "Oven depths", 15, 20000.0},
    };

    for (const auto& d : defs) {
        Dungeon dun;
        dun.id = d.id;
        dun.name = d.name;
        dun.maxFloor = d.maxFloors;
        dungeons[d.id] = dun;
        nextDungeonId = std::max(nextDungeonId, d.id + 1);
    }
}

void Dungeons::init() {
    registerDungeons();
    loaded = true;
    unlocked = true;
    level = 1;
}

void Dungeons::eachFrame() {
    if (!loaded || !unlocked) return;
    advanceProgress(1.0 / static_cast<f64>(GameConst::FPS));
}

const Dungeons::Dungeon* Dungeons::dungeonAt(i32 idx) const {
    i32 i = 0;
    for (auto& kv : dungeons) {
        if (i == idx) return &kv.second;
        ++i;
    }
    return nullptr;
}

bool Dungeons::startDungeon(i32 dungeonId) {
    auto it = dungeons.find(dungeonId);
    if (it == dungeons.end()) return false;
    if (it->second.completed) return false;
    if (activeDungeonId != -1) return false; // one dungeon at a time
    activeDungeonId = dungeonId;
    return true;
}

f64 Dungeons::exploreRatePerFrame() const {
    // Progress per frame; a full floor takes ~30 seconds at 30 FPS
    return 100.0 / (30.0 * static_cast<f64>(GameConst::FPS));
}

void Dungeons::advanceProgress(f64 /*dtSeconds*/) {
    if (activeDungeonId == -1) return;
    auto it = dungeons.find(activeDungeonId);
    if (it == dungeons.end()) return;

    auto& dun = it->second;
    if (dun.completed) return;

    dun.progress += static_cast<i32>(exploreRatePerFrame());
    if (dun.progress >= 100) {
        dun.progress = 0;
        ++dun.floor;
        ++dun.monstersDefeated;
        if (dun.floor > dun.maxFloor) dun.maxFloor = dun.floor;
        if (dun.floor >= dun.maxFloor + 5) {
            // Arbitrary completion threshold: clear after maxFloors + 5
            dun.completed = true;
            dun.progress = 100;
            dun.rewardCookies += static_cast<i32>(1000.0 * dun.maxFloor);
            activeDungeonId = -1;
        } else {
            // Reward for reaching a new floor
            dun.rewardCookies += static_cast<i32>(100.0 * dun.floor);
        }
    }
}

i32 Dungeons::claimRewards(i32 dungeonId) {
    auto it = dungeons.find(dungeonId);
    if (it == dungeons.end()) return 0;
    i32 reward = it->second.rewardCookies;
    it->second.rewardCookies = 0;
    if (g_game && reward > 0) g_game->earn(static_cast<f64>(reward));
    return reward;
}

f64 Dungeons::cookieBonus() const {
    if (!loaded || !unlocked) return 1.0;
    // Small CPS bonus per completed dungeon
    f64 mult = 1.0;
    for (const auto& kv : dungeons) {
        if (kv.second.completed) mult *= 1.02;
    }
    return mult;
}

f64 Dungeons::clickBonus() const { return 1.0; }

i32 Dungeons::maxSlots() const { return static_cast<i32>(dungeons.size()); }
i32 Dungeons::slotCount() const {
    i32 n = 0;
    for (const auto& kv : dungeons) if (!kv.second.completed) ++n;
    return n;
}

std::string Dungeons::save() {
    std::ostringstream s;
    s << level << ";";
    for (const auto& kv : dungeons) {
        const auto& d = kv.second;
        s << d.id << ","
          << d.floor << ","
          << d.maxFloor << ","
          << d.progress << ","
          << (d.completed ? 1 : 0) << ","
          << d.rewardCookies << ";";
    }
    s << activeDungeonId;
    return s.str();
}

void Dungeons::load(const std::string& data) {
    std::istringstream ss(data);
    std::string token;
    if (std::getline(ss, token, ';')) level = std::stoi(token);

    while (std::getline(ss, token, ';')) {
        if (token.empty()) break;
        // Format: id,floor,maxFloor,progress,completed,rewardCookies
        size_t c1 = token.find(',');
        size_t c2 = token.find(',', c1 + 1);
        size_t c3 = token.find(',', c2 + 1);
        size_t c4 = token.find(',', c3 + 1);
        size_t c5 = token.find(',', c4 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos ||
            c3 == std::string::npos || c4 == std::string::npos ||
            c5 == std::string::npos)
        {
            // Last token is activeDungeonId (no commas)
            activeDungeonId = std::stoi(token);
            break;
        }
        Dungeon d;
        d.id = std::stoi(token.substr(0, c1));
        d.floor = std::stoi(token.substr(c1 + 1, c2 - c1 - 1));
        d.maxFloor = std::stoi(token.substr(c2 + 1, c3 - c2 - 1));
        d.progress = std::stoi(token.substr(c3 + 1, c4 - c3 - 1));
        d.completed = (std::stoi(token.substr(c4 + 1, c5 - c4 - 1)) != 0);
        d.rewardCookies = std::stoi(token.substr(c5 + 1));
        dungeons[d.id] = d;
    }
    loaded = true;
    unlocked = true;
}
