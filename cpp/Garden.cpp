#include "Garden.h"

Garden::Garden(Building* parent) { this->parent = parent; name = "Garden"; }
void Garden::init() { loaded = true; }
void Garden::draw() {}
void Garden::eachFrame() {}
std::string Garden::save() { return ""; }
