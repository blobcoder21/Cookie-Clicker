#pragma once

#include "Minigame.h"

class Grimoire : public Minigame {
public:
    explicit Grimoire(Building* parent);
    void init() override;
};
