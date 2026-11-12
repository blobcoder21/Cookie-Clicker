#pragma once

#include "Minigame.h"
#include "Types.h"
#include <vector>
#include <map>
#include <string>

class Pantheon : public Minigame {
public:
    explicit Pantheon(Building* parent);
    void init() override;
    void eachFrame() override;
    std::string save() override;
    void load(const std::string& data) override;
    f64 cookieBonus() const override;
    f64 clickBonus() const override;
    i32 maxSlots() const override;
    i32 slotCount() const override;

    // God definitions (read-only for UI)
    struct God {
        i32 id;
        std::string name;
        f64 cpsMult;        // CpS multiplier when slotted
        f64 clickMult;      // click power multiplier when slotted
        f64 drawback;       // penalty (0 = none)
        bool unlocked;
    };

    static constexpr i32 maxGodSlots = 3;

    i32 godSlotCount() const { return maxGodSlots; }
    i32 slottedGod(i32 slot) const;            // god id in slot, -1 = empty
    const God* godDef(i32 godId) const;
    f64 divinePower() const { return divinePower_; }
    f64 maxDivinePower() const { return maxDivinePower_; }

    // Slot a god into a slot (core logic; UI calls this later)
    bool slotGod(i32 slot, i32 godId);
    bool unslotGod(i32 slot);

private:
    f64 divinePower_ = 0.0;
    f64 maxDivinePower_ = 100.0;
    i32 godSlots[maxGodSlots] = {-1, -1, -1};

    std::map<i32, God> gods;
    i32 nextGodId = 0;

    void registerGods();
};
