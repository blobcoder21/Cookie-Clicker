#include "Pantheon.h"

Pantheon::Pantheon(Building* parent) { this->parent = parent; name = "Pantheon"; }
void Pantheon::init() { loaded = true; }
