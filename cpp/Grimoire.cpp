#include "Grimoire.h"

Grimoire::Grimoire(Building* parent) { this->parent = parent; name = "Grimoire"; }
void Grimoire::init() { loaded = true; }
