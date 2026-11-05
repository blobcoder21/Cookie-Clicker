#include "Dungeons.h"

Dungeons::Dungeons(Building* parent) { this->parent = parent; name = "Dungeons"; }
void Dungeons::init() { loaded = true; }
