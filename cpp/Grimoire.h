#pragma once

#include "Minigame.h"
#include "Types.h"
#include <vector>
#include <map>
#include <string>

class Grimoire : public Minigame {
public:
    explicit Grimoire(Building* parent);
    void init() override;
    void eachFrame() override;
    std::string save() override;
    void load(const std::string& data) override;
    f64 cookieBonus() const override;
    f64 clickBonus() const override;
    i32 maxSlots() const override;
    i32 slotCount() const override;

    // Spell definitions (read-only for UI)
    struct Spell {
        i32 id;
        std::string name;
        f64 manaCost;      // mana required to cast
        f64 cooldown;      // seconds between casts
        f64 cpsMult;       // temporary CPS multiplier when active
        f64 duration;      // seconds the effect lasts
        bool unlocked;
        bool castable;     // off cooldown and enough mana
    };

    i32 spellCount() const { return static_cast<i32>(spellStates.size()); }
    const Spell* spellAt(i32 idx) const;
    f64 mana() const { return mana_; }
    f64 maxMana() const { return maxMana_; }

    // Cast a spell by index (core logic; UI calls this later)
    bool castSpell(i32 idx);

private:
    f64 mana_ = 0.0;
    f64 maxMana_ = 100.0;
    f64 manaRegenPerFrame = 0.0;

    struct SpellState {
        Spell def;
        f64 cooldownRemaining = 0.0;
        f64 effectRemaining = 0.0;  // seconds the cast effect is still active
    };
    std::map<i32, SpellState> spellStates;
    i32 nextSpellId = 0;

    void registerSpells();
    f64 activeCpsMult() const;
};
