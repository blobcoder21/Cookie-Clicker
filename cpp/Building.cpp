#include "Building.h"
#include "Game.h"

Building::Building(std::string name, std::string commonName, std::string desc,
                   std::string icon, i32 iconColumn, Art art,
                   f64 price, f64 cps, std::function<void(Building*)> buyFunc)
    : name(std::move(name)), displayName(name),
      desc(std::move(desc)), basePrice(price), price(price), bulkPrice(price),
      cps(cps), baseCps(cps), icon(std::move(icon)), iconColumn(iconColumn),
      art(std::move(art)), buyFunction(std::move(buyFunc))
{
    // commonName is "single|plural|action|extra|extraPlural" in JS
    auto parts = commonName;
    // minimal split on '|'
    size_t pos = 0;
    std::string token;
    std::vector<std::string> out;
    while ((pos = parts.find('|')) != std::string::npos) {
        token = parts.substr(0, pos);
        out.push_back(token);
        parts.erase(0, pos + 1);
    }
    out.push_back(parts);
    if (!out.empty()) single = out[0];
    if (out.size() > 1) plural = out[1];
    if (out.size() > 2) actionName = out[2];
    if (out.size() > 3) extraName = out[3];
    if (out.size() > 4) extraPlural = out[4];

    // JS base price curve for id != 0 is applied in Game when building is
    // constructed; kept here as a helper for data-driven building defs.
}

f64 Building::getPrice(i32 n) const {
    i32 amount = (n == -1) ? this->amount : n;
    f64 price = basePrice * std::pow(GameConst::PRICE_INCREASE,
                                     std::max(0.0, static_cast<f64>(amount - free)));
    return std::ceil(price);
}

f64 Building::getSumPrice(i32 amount) const {
    f64 price = 0.0;
    for (i32 i = 0; i < amount; ++i) {
        price += getPrice(static_cast<i32>(this->amount + i));
    }
    return price;
}

f64 Building::getReverseSumPrice(i32 amount) const {
    f64 price = 0.0;
    for (i32 i = std::max(0, this->amount - amount);
         i < std::max(0, this->amount); ++i) {
        price += getPrice(i);
    }
    price *= getSellMultiplier();
    return std::ceil(price);
}

f64 Building::getSellMultiplier() const {
    f64 giveBack = 0.25;
    return giveBack;
}

void Building::buy(i32 amount) {
    if (amount <= 0) amount = 1;
    // Actual cookie deduction handled by Game::buyBuilding; this just updates
    // the building's own counters and triggers its buyFunction.
    for (i32 i = 0; i < amount; ++i) {
        this->amount++;
        this->bought++;
        price = getPrice();
        bulkPrice = price;
        if (buyFunction) buyFunction(this);
    }
}

void Building::sell(i32 amount, bool /*bypass*/) {
    if (amount == -1) amount = this->amount;
    if (amount <= 0) amount = 1;
    i32 sold = 0;
    for (i32 i = 0; i < amount; ++i) {
        if (this->amount <= 0) break;
        f64 price = getPrice();
        f64 giveBack = getSellMultiplier();
        price = std::floor(price * giveBack);
        this->amount--;
        price = getPrice();
        bulkPrice = price;
        sold++;
    }
    (void)sold;
}

void Building::refresh() {
    // placeholder for UI refresh + price/cps recalc
}

void Building::switchMinigame(bool on) {
    onMinigame = on;
    refresh();
}
