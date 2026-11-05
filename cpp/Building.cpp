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
    // Game.modifyBuildingPrice hook would go here
    return std::ceil(price);
}

f64 Building::getSumPrice(i32 amount) const {
    f64 price = 0.0;
    for (i32 i = 0; i < amount; ++i) {
        price += getPrice(static_cast<i32>(this->amount + i));
    }
    return price;
}

void Building::refresh() {
    // placeholder for UI refresh + price/cps recalc
}

void Building::switchMinigame(bool on) {
    // on == -1 means toggle (not representable via bool; callers handle it)
    onMinigame = on;
    refresh();
}
