#include "Achievement.h"

Achievement::Achievement(std::string name, std::string desc, std::string icon)
    : name(std::move(name)), desc(std::move(desc)), icon(std::move(icon)) {}
