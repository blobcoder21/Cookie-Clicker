#pragma once

#include "Minigame.h"

class Dungeons : public Minigame {
public:
    explicit Dungeons(Building* parent);
    void init() override;
};
