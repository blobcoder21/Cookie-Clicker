#include "Game.h"
#include "SaveLoad.h"
#include <algorithm>

Game* g_game = nullptr;

Game::Game() {
    g_game = this;
}

Game::~Game() {
    g_game = nullptr;
}

void Game::launch() {
    time = static_cast<f64>(now_ms());
    lastDate = time;
    startDate = time;
    fullDate = time;
    bakeryName = getBakeryName();
    seed = std::to_string(static_cast<i64>(time)); // placeholder seed
    // TODO: load assets, init buildings/upgrades/achievements from PRESETS data
    ready = true;
}

void Game::load() {
    // asset loading + data tables; called from window.onload analogue
}

void Game::readyToLaunch() {
    // end of init
}

void Game::loop() {
    f64 now = static_cast<f64>(now_ms());
    f64 dt = (now - time) / 1000.0;
    time = now;
    if (dt > 0.25) dt = 0.25; // spiral-of-death clamp
    logic(dt);
    draw();
    loopT++;
    drawT++;
}

void Game::logic(f64 dt) {
    (void)dt;
    if (onAscend || ascendTimer > 0.0) return;

    // per-frame building updates
    for (auto& b : objects) {
        if (b && b->eachFrame) b->eachFrame();
    }

    calculateGains();
    updateSpecial();
    updateGrandmapocalypse();
    particlesUpdate();
}

void Game::draw() {
    // TODO: ImGui + OpenGL rendering of cookie panel, store, etc.
    (void)0;
}

void Game::earn(f64 howmuch) {
    cookies += howmuch;
    cookiesEarned += howmuch;
}

void Game::spend(f64 howmuch) {
    cookies -= howmuch;
}

void Game::dissolve(f64 howmuch) {
    cookies -= howmuch;
    cookiesEarned -= howmuch;
    cookies = std::max(0.0, cookies);
    cookiesEarned = std::max(0.0, cookiesEarned);
}

f64 Game::mouseCps() {
    f64 add = 0.0;
    if (has("Thousand fingers")) add += 0.1;
    if (has("Million fingers")) add += 0.5;
    if (has("Billion fingers")) add += 5.0;
    if (has("Trillion fingers")) add += 50.0;
    i32 num = 0;
    for (auto& b : objects) num += b->amount;
    if (auto* cur = getBuilding("Cursor")) num -= cur->amount;
    add *= static_cast<f64>(num);

    f64 mult = 1.0;
    for (auto& cb : customMouseCps) mult += cb();
    for (auto& cb : customMouseCpsMult) mult *= cb();

    if (has("Santa's helpers")) mult *= 1.1;
    if (has("Cookie egg")) mult *= 1.1;
    mult *= eff("click");
    return mult * add;
}

void Game::clickCookie(i32 amount) {
    if (onAscend || ascendTimer > 0.0) return;
    f64 now = static_cast<f64>(now_ms());
    if (now - lastClick < 1000.0 / 250.0) return; // min click interval
    lastClick = now;
    f64 amt = amount ? static_cast<f64>(amount) : mouseCps();
    earn(amt);
    handmadeCookies += amt;
    cookieClicks++;
    // TODO: particles, sound
}

void Game::calculateGains() {
    // TODO: full port of Game.CalculateGains (building CpS + mults + buffs)
    cookiesPs = 0.0;
    for (auto& b : objects) {
        cookiesPs += b->amount * b->cps;
    }
}

bool Game::has(const std::string& name) const {
    for (auto& u : upgrades) if (u && u->name == name) return u->bought;
    return false;
}

bool Game::hasAchiev(const std::string& name) const {
    for (auto& a : achievements) if (a && a->name == name) return a->won;
    return false;
}

void Game::unlock(const std::string& name) {
    for (auto& u : upgrades) if (u && u->name == name) u->unlocked = true;
}

Building* Game::getBuilding(const std::string& name) {
    for (auto& b : objects) if (b && b->name == name) return b.get();
    return nullptr;
}

std::string Game::getBakeryName() {
    return "Cookie Bakery";
}

void Game::updateSpecial() { /* TODO */ }
void Game::updateGrandmapocalypse() { /* TODO */ }
void Game::particlesUpdate() { /* TODO */ }
void Game::textParticlesUpdate() { /* TODO */ }

f64 Game::eff(const std::string& name, f64 def) {
    auto it = effs_.find(name);
    return (it != effs_.end()) ? it->second : def;
}

std::string Game::writeSave(i32 type) { return SaveLoad::writeSave(*this, type); }
bool Game::loadSave(const std::string& data) { return SaveLoad::loadSave(*this, data); }
bool Game::importSaveCode(const std::string& code) { return loadSave(code); }
