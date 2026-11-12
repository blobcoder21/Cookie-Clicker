#include "Pantheon.h"
#include "Game.h"
#include <algorithm>
#include <sstream>

Pantheon::Pantheon(Building* parent) { this->parent = parent; name = "Pantheon"; }

void Pantheon::registerGods() {
    // Starter god set (mirrors JS Pantheon gods, simplified).
    const struct {
        i32 id;
        const char* name;
        f64 cpsMult;
        f64 clickMult;
        f64 drawback;
    } defs[] = {
        {1, "Aphrodite", 1.1, 1.0, 0.0},
        {2, "Baldur", 1.0, 1.15, 0.0},
        {3, "Cerberus", 1.05, 1.05, 0.02},
        {4, "Desdemona", 1.2, 0.9, 0.05},
        {5, "Echidna", 1.15, 1.0, 0.03},
        {6, "Gaia", 1.1, 1.0, 0.0},
    };

    for (const auto& d : defs) {
        God g;
        g.id = d.id;
        g.name = d.name;
        g.cpsMult = d.cpsMult;
        g.clickMult = d.clickMult;
        g.drawback = d.drawback;
        g.unlocked = true;
        gods[d.id] = g;
        nextGodId = std::max(nextGodId, d.id + 1);
    }

    maxDivinePower_ = 100.0;
    divinePower_ = maxDivinePower_ / 2.0;
}

void Pantheon::init() {
    registerGods();
    loaded = true;
    unlocked = true;
    level = 1;
}

void Pantheon::eachFrame() {
    if (!loaded || !unlocked) return;
    // Slowly regenerate divine power
    divinePower_ += 0.1 / static_cast<f64>(GameConst::FPS);
    if (divinePower_ > maxDivinePower_) divinePower_ = maxDivinePower_;
}

i32 Pantheon::slottedGod(i32 slot) const {
    if (slot < 0 || slot >= maxGodSlots) return -1;
    return godSlots[slot];
}

const Pantheon::God* Pantheon::godDef(i32 godId) const {
    auto it = gods.find(godId);
    return (it != gods.end()) ? &it->second : nullptr;
}

bool Pantheon::slotGod(i32 slot, i32 godId) {
    if (slot < 0 || slot >= maxGodSlots) return false;
    auto it = gods.find(godId);
    if (it == gods.end() || !it->second.unlocked) return false;
    // Can't slot the same god twice
    for (i32 i = 0; i < maxGodSlots; ++i) {
        if (godSlots[i] == godId) return false;
    }
    godSlots[slot] = godId;
    return true;
}

bool Pantheon::unslotGod(i32 slot) {
    if (slot < 0 || slot >= maxGodSlots) return false;
    if (godSlots[slot] == -1) return false;
    godSlots[slot] = -1;
    return true;
}

f64 Pantheon::cookieBonus() const {
    if (!loaded || !unlocked) return 1.0;
    f64 mult = 1.0;
    f64 drawback = 0.0;
    for (i32 i = 0; i < maxGodSlots; ++i) {
        i32 gid = godSlots[i];
        if (gid == -1) continue;
        const God* g = godDef(gid);
        if (!g) continue;
        mult *= g->cpsMult;
        drawback += g->drawback;
    }
    // Apply drawback as a reduction (max 50% penalty)
    f64 penalty = 1.0 - Math::clamp(drawback, 0.0, 0.5);
    return mult * penalty;
}

f64 Pantheon::clickBonus() const {
    if (!loaded || !unlocked) return 1.0;
    f64 mult = 1.0;
    for (i32 i = 0; i < maxGodSlots; ++i) {
        i32 gid = godSlots[i];
        if (gid == -1) continue;
        const God* g = godDef(gid);
        if (g) mult *= g->clickMult;
    }
    return mult;
}

i32 Pantheon::maxSlots() const { return maxGodSlots; }
i32 Pantheon::slotCount() const {
    i32 n = 0;
    for (i32 i = 0; i < maxGodSlots; ++i) if (godSlots[i] != -1) ++n;
    return n;
}

std::string Pantheon::save() {
    std::ostringstream s;
    s << level << ";";
    for (i32 i = 0; i < maxGodSlots; ++i) {
        s << godSlots[i] << ";";
    }
    s << static_cast<i32>(divinePower_);
    return s.str();
}

void Pantheon::load(const std::string& data) {
    std::istringstream ss(data);
    std::string token;
    if (std::getline(ss, token, ';')) level = std::stoi(token);
    for (i32 i = 0; i < maxGodSlots; ++i) {
        if (!std::getline(ss, token, ';')) break;
        godSlots[i] = std::stoi(token);
    }
    if (std::getline(ss, token, ';')) divinePower_ = static_cast<f64>(std::stoi(token));
    if (divinePower_ > maxDivinePower_) divinePower_ = maxDivinePower_;
    loaded = true;
    unlocked = true;
}
