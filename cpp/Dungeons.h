#pragma once

#include "Minigame.h"
#include "Types.h"
#include <vector>
#include <string>
#include <map>

class Dungeons : public Minigame {
public:
    explicit Dungeons(Building* parent);
    void init() override;
    void eachFrame() override;
    std::string save() override;
    void load(const std::string& data) override;
    f64 cookieBonus() const override;
    f64 clickBonus() const override;
    i32 maxSlots() const override;
    i32 slotCount() const override;

    // Dungeon state (read-only for UI)
    struct Dungeon {
        i32 id;
        std::string name;
        i32 floor = 1;           // current floor
        i32 maxFloor = 1;        // deepest floor reached
        i32 progress = 0;        // progress on current floor (0..100)
        bool completed = false;  // dungeon fully cleared
        i32 rewardCookies = 0;   // pending cookie reward
        i32 monstersDefeated = 0;
    };

    i32 dungeonCount() const { return static_cast<i32>(dungeons.size()); }
    const Dungeon* dungeonAt(i32 idx) const;
    i32 activeDungeon() const { return activeDungeonId; }

    // Start a dungeon run (core logic; UI calls this later)
    bool startDungeon(i32 dungeonId);
    // Advance progress on the active dungeon (called each frame while exploring)
    void advanceProgress(f64 dtSeconds);
    // Claim rewards from a completed dungeon
    i32 claimRewards(i32 dungeonId);

private:
    i32 activeDungeonId = -1;
    std::map<i32, Dungeon> dungeons;
    i32 nextDungeonId = 0;

    void registerDungeons();
    f64 exploreRatePerFrame() const;
};
