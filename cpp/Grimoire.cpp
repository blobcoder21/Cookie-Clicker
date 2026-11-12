#include "Grimoire.h"
#include "Game.h"
#include <algorithm>
#include <sstream>

Grimoire::Grimoire(Building* parent) { this->parent = parent; name = "Grimoire"; }

void Grimoire::registerSpells() {
    // Starter spell set (mirrors JS Grimoire spells, simplified).
    // id, name, manaCost, cooldown(sec), cpsMult, duration(sec)
    const struct {
        i32 id;
        const char* name;
        f64 manaCost;
        f64 cooldown;
        f64 cpsMult;
        f64 duration;
    } defs[] = {
        {1, "Conjure Baked Goods", 50.0, 60.0, 0.0, 0.0},   // instant cookie reward
        {2, "Spell of Fervor", 30.0, 30.0, 1.5, 30.0},       // +50% CPS for 30s
        {3, "Spider Babies", 20.0, 20.0, 1.2, 15.0},         // +20% CPS for 15s
    };

    for (const auto& d : defs) {
        Spell s;
        s.id = d.id;
        s.name = d.name;
        s.manaCost = d.manaCost;
        s.cooldown = d.cooldown;
        s.cpsMult = d.cpsMult;
        s.duration = d.duration;
        s.unlocked = true;
        s.castable = true;

        SpellState st;
        st.def = s;
        st.cooldownRemaining = 0.0;
        st.effectRemaining = 0.0;
        spellStates[d.id] = st;
        nextSpellId = std::max(nextSpellId, d.id + 1);
    }

    maxMana_ = 100.0;
    mana_ = maxMana_;
    // regenerate full mana over ~2 minutes
    manaRegenPerFrame = maxMana_ / (120.0 * static_cast<f64>(GameConst::FPS));
}

void Grimoire::init() {
    registerSpells();
    loaded = true;
    unlocked = true;
    level = 1;
}

const Grimoire::Spell* Grimoire::spellAt(i32 idx) const {
    i32 i = 0;
    for (auto& kv : spellStates) {
        if (i == idx) return &kv.second.def;
        ++i;
    }
    return nullptr;
}

bool Grimoire::castSpell(i32 idx) {
    const Spell* s = spellAt(idx);
    if (!s || !s->unlocked) return false;
    if (mana_ < s->manaCost) return false;

    auto it = spellStates.find(s->id);
    if (it == spellStates.end()) return false;
    if (it->second.cooldownRemaining > 0.0) return false;

    // Pay mana
    mana_ -= s->manaCost;

    // Start cooldown
    it->second.cooldownRemaining = s->cooldown;

    // Apply effect
    if (s->cpsMult > 1.0 && s->duration > 0.0) {
        it->second.effectRemaining = s->duration;
    } else if (s->id == 1) {
        // Conjure Baked Goods: instant cookie reward based on CPS
        f64 reward = g_game ? g_game->cookiesPs * 30.0 : 0.0;
        if (g_game) g_game->earn(reward);
    }

    return true;
}

void Grimoire::eachFrame() {
    if (!loaded || !unlocked) return;

    // Regenerate mana
    mana_ += manaRegenPerFrame;
    if (mana_ > maxMana_) mana_ = maxMana_;

    // Update cooldowns and active effects
    for (auto& kv : spellStates) {
        auto& st = kv.second;
        if (st.cooldownRemaining > 0.0) {
            st.cooldownRemaining -= 1.0 / static_cast<f64>(GameConst::FPS);
            if (st.cooldownRemaining < 0.0) st.cooldownRemaining = 0.0;
        }
        if (st.effectRemaining > 0.0) {
            st.effectRemaining -= 1.0 / static_cast<f64>(GameConst::FPS);
            if (st.effectRemaining < 0.0) st.effectRemaining = 0.0;
        }
    }

    // Update castable flags
    for (auto& kv : spellStates) {
        auto& st = kv.second;
        st.def.castable = st.def.unlocked &&
                          mana_ >= st.def.manaCost &&
                          st.cooldownRemaining <= 0.0;
    }
}

f64 Grimoire::activeCpsMult() const {
    f64 mult = 1.0;
    for (const auto& kv : spellStates) {
        if (kv.second.effectRemaining > 0.0) {
            mult *= kv.second.def.cpsMult;
        }
    }
    return mult;
}

f64 Grimoire::cookieBonus() const {
    if (!loaded || !unlocked) return 0.0;
    // Grimoire provides a base magic CPS multiplier plus active spell effects.
    f64 baseMult = 1.0 + (static_cast<f64>(level) * 0.05); // +5% per level
    return baseMult * activeCpsMult();
}

f64 Grimoire::clickBonus() const { return 0.0; }
i32 Grimoire::maxSlots() const { return static_cast<i32>(spellStates.size()); }
i32 Grimoire::slotCount() const {
    i32 n = 0;
    for (const auto& kv : spellStates) if (kv.second.def.unlocked) ++n;
    return n;
}

std::string Grimoire::save() {
    std::ostringstream s;
    s << level << ";" << static_cast<i32>(mana_) << ";";
    for (const auto& kv : spellStates) {
        const auto& st = kv.second;
        s << st.def.id << ","
          << static_cast<i32>(st.cooldownRemaining * 10.0) << ","
          << static_cast<i32>(st.effectRemaining * 10.0) << ";";
    }
    return s.str();
}

void Grimoire::load(const std::string& data) {
    std::istringstream ss(data);
    std::string token;
    if (std::getline(ss, token, ';')) level = std::stoi(token);
    if (std::getline(ss, token, ';')) mana_ = static_cast<f64>(std::stoi(token));
    if (mana_ > maxMana_) mana_ = maxMana_;

    while (std::getline(ss, token, ';')) {
        if (token.empty()) break;
        size_t c1 = token.find(',');
        size_t c2 = token.find(',', c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos) continue;
        i32 id = std::stoi(token.substr(0, c1));
        i32 cd = std::stoi(token.substr(c1 + 1, c2 - c1 - 1));
        i32 eff = std::stoi(token.substr(c2 + 1));
        auto it = spellStates.find(id);
        if (it != spellStates.end()) {
            it->second.cooldownRemaining = static_cast<f64>(cd) / 10.0;
            it->second.effectRemaining = static_cast<f64>(eff) / 10.0;
        }
    }
    loaded = true;
    unlocked = true;
}
