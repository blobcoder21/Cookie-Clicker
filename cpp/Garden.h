#pragma once

#include "Minigame.h"
#include "Types.h"
#include <vector>
#include <array>
#include <map>

class Garden : public Minigame {
public:
    explicit Garden(Building* parent);
    void init() override;
    void eachFrame() override;
    std::string save() override;
    void load(const std::string& data) override;
    f64 cookieBonus() const override;
    f64 clickBonus() const override;
    i32 maxSlots() const override;
    i32 slotCount() const override;

    // Plot grid access (read-only for UI)
    i32 width() const { return gridW; }
    i32 height() const { return gridH; }
    i32 plantAt(i32 x, i32 y) const;       // plant id at plot, -1 = empty
    i32 growAt(i32 x, i32 y) const;         // growth stage 0..100

    // Actions (called from UI later; core logic only here)
    bool plant(i32 x, i32 y, i32 plantId);
    bool harvest(i32 x, i32 y);
    bool mutate(i32 x, i32 y);

private:
    static constexpr i32 gridW = 6;
    static constexpr i32 gridH = 6;
    static constexpr i32 totalPlots = gridW * gridH;

    // Plant definitions (mirrors JS Garden plant list, starter subset)
    struct PlantDef {
        i32 id;
        std::string name;
        f64 baseCookieBonus;   // CpS bonus per mature plant of this type
        f64 growTime;          // seconds to reach maturity
        f64 mutationChance;    // per-frame chance to mutate when adjacent to a different plant
    };

    struct Plot {
        i32 plantId = -1;      // -1 = empty
        f64 growth = 0.0;      // 0.0 .. 1.0
        i32 age = 0;           // frames since planted
    };

    std::array<Plot, totalPlots> plots;
    std::map<i32, PlantDef> plantDefs;
    i32 plantSeed = 0;         // next plant id to assign
    Prng rng_;                 // seeded PRNG for deterministic mutations

    void registerPlants();
    f64 plantGrowRate(i32 plantId) const;
    bool tryMutate(i32 x, i32 y);
    i32 indexOf(i32 x, i32 y) const { return y * gridW + x; }
    bool valid(i32 x, i32 y) const { return x >= 0 && x < gridW && y >= 0 && y < gridH; }
};
