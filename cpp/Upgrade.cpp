#include "Upgrade.h"
#include "Game.h"

Upgrade::Upgrade(std::string name, std::string desc, f64 price, std::string icon,
                 std::function<bool()> buyFunc)
    : name(std::move(name)), desc(std::move(desc)), price(price),
      icon(std::move(icon)), buyFunction(std::move(buyFunc)) {}

bool Upgrade::canBuy() const {
    return unlocked && !bought;
}
