#include "Shimmer.h"

Shimmer::Shimmer(std::string type, f64 x, f64 y, f64 dur)
    : type(std::move(type)), x(x), y(y), dur(dur) {}

void Shimmer::tick(f64 dt) {
    age += dt;
    if (age >= dur) dead = true;
}
